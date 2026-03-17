#include "elk.h"
#include "../../include/serial.h"
#include "../libc/stdbool.h"
#include "../libc/stdint.h"
#include "../libc/stddef.h"
#include "../libc/stdio.h"
#include "../../include/stdlib.h"
#include "../../include/string.h"

#define assert(x)

typedef uint32_t jsoff_t;

struct js {
  jsoff_t css;        // Max observed C stack size
  jsoff_t lwm;        // JS RAM low watermark: min free RAM observed
  const char *code;   // Currently parsed code snippet
  char errmsg[33];    // Error message placeholder
  uint8_t tok;        // Last parsed token value
  uint8_t consumed;   // Indicator that last parsed token was consumed
  uint8_t flags;      // Execution flags, see F_* constants below
  jsoff_t clen;       // Code snippet length
  jsoff_t pos;        // Current parsing position
  jsoff_t toff;       // Offset of the last parsed token
  jsoff_t tlen;       // Length of the last parsed token
  jsoff_t nogc;       // Entity offset to exclude from GC
  jsval_t tval;       // Holds last parsed numeric or string literal value
  jsval_t scope;      // Current scope
  uint8_t *mem;       // Available JS memory
  jsoff_t size;       // Memory size
  jsoff_t brk;        // Current mem usage boundary
  jsoff_t gct;        // GC threshold. If brk > gct, trigger GC
  jsoff_t maxcss;     // Maximum allowed C stack size usage
  void *cstk;         // C stack pointer at the beginning of js_eval()
};

// Execution flags
#define F_CALL 1U
#define F_NOEXEC 2U
#define F_LOOP 4U
#define F_BREAK 8U
#define F_RETURN 16U

enum { 
  TOK_ERR, TOK_EOF, TOK_IDENTIFIER, TOK_NUMBER, TOK_STRING, TOK_SEMICOLON,
  TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
  TOK_BREAK = 50, TOK_CASE, TOK_CATCH, TOK_CLASS, TOK_CONST, TOK_CONTINUE,
  TOK_DEFAULT, TOK_DELETE, TOK_DO, TOK_ELSE, TOK_FINALLY, TOK_FOR, TOK_FUNC,
  TOK_IF, TOK_IN, TOK_INSTANCEOF, TOK_LET, TOK_NEW, TOK_RETURN, TOK_SWITCH,
  TOK_THIS, TOK_THROW, TOK_TRY, TOK_VAR, TOK_VOID, TOK_WHILE, TOK_WITH,
  TOK_YIELD, TOK_UNDEF, TOK_NULL, TOK_TRUE, TOK_FALSE,
  TOK_DOT = 100, TOK_CALL, TOK_POSTINC, TOK_POSTDEC, TOK_NOT, TOK_TILDA,
  TOK_TYPEOF, TOK_UPLUS, TOK_UMINUS, TOK_EXP, TOK_MUL, TOK_DIV, TOK_REM,
  TOK_PLUS, TOK_MINUS, TOK_SHL, TOK_SHR, TOK_ZSHR, TOK_LT, TOK_LE, TOK_GT,
  TOK_GE, TOK_EQ, TOK_NE, TOK_AND, TOK_XOR, TOK_OR, TOK_LAND, TOK_LOR,
  TOK_COLON, TOK_Q,  TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN,
  TOK_MUL_ASSIGN, TOK_DIV_ASSIGN, TOK_REM_ASSIGN, TOK_SHL_ASSIGN,
  TOK_SHR_ASSIGN, TOK_ZSHR_ASSIGN, TOK_AND_ASSIGN, TOK_XOR_ASSIGN,
  TOK_OR_ASSIGN, TOK_COMMA,
};

static struct js *js_ctx_global = NULL;

enum {
  T_OBJ, T_PROP, T_STR, T_UNDEF, T_NULL, T_NUM, T_BOOL, T_FUNC, T_CODEREF,
  T_CFUNC, T_ERR
};

static uint8_t vtype(jsval_t v);
static size_t vdata(jsval_t v);
static jsval_t mkval(uint8_t type, uint64_t data) { return ((jsval_t) 0x7ff0U << 48U) | ((jsval_t) (type) << 48) | (data & 0xffffffffffffUL); }
static bool is_nan(jsval_t v) { return (v >> 52U) == 0x7ffU; }
static uint8_t vtype(jsval_t v) { return is_nan(v) ? ((v >> 48U) & 15U) : (uint8_t) T_NUM; }
static size_t vdata(jsval_t v) { return (size_t) (v & ~((jsval_t) 0x7fffUL << 48U)); }

static jsval_t tov(double d) { union { double d; jsval_t v; } u = {d}; return u.v; }
static double tod(jsval_t v) { union { jsval_t v; double d; } u = {v}; return u.d; }

#define GCMASK ~(((jsoff_t) ~0) >> 1)
#define TOK(_t, _l) do { js->tok = _t; js->tlen = _l; return js->tok; } while (0)
#define LOOK(OFS, CH) js->toff + OFS < js->clen && buf[OFS] == CH
#define EXPECT(_tok, _e) do { if (next(js) != _tok) { _e; return js_mkerr(js, "parse error"); }; js->consumed = 1; } while (0)

static uint8_t next(struct js *js);
static jsval_t js_stmt(struct js *js);
static jsval_t js_expr(struct js *js);
jsval_t js_mkerr(struct js *js, const char *fmt, ...);
static jsval_t do_op(struct js *js, uint8_t op, jsval_t l, jsval_t r);
static jsoff_t lkp(struct js *js, jsval_t obj, const char *buf, size_t len);
static jsval_t setprop(struct js *js, jsval_t obj, jsval_t k, jsval_t v);
static jsoff_t skiptonext(const char *code, jsoff_t len, jsoff_t n);
static uint8_t parseident(const char *buf, jsoff_t len, jsoff_t *tlen);

static bool is_space(int c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f' || c == '\v'; }
static bool is_ident_begin(int c) { return c == '_' || c == '$' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool is_ident_continue(int c) { return is_ident_begin(c) || (c >= '0' && c <= '9'); }

static jsoff_t loadoff(struct js *js, jsoff_t off) { jsoff_t v = 0; memcpy(&v, &js->mem[off], sizeof(v)); return v; }
static void saveoff(struct js *js, jsoff_t off, jsoff_t val) { memcpy(&js->mem[off], &val, sizeof(val)); }
static jsval_t loadval(struct js *js, jsoff_t off) { jsval_t v = 0; memcpy(&v, &js->mem[off], sizeof(v)); return v; }
static void saveval(struct js *js, jsoff_t off, jsval_t val) { memcpy(&js->mem[off], &val, sizeof(val)); }
static jsoff_t offtolen(jsoff_t off) { return (off >> 2) - 1; }
static jsoff_t align32(jsoff_t v) { return ((v + 3) >> 2) << 2; }


static jsoff_t js_alloc(struct js *js, size_t size) {
  jsoff_t ofs = js->brk;
  size = align32((jsoff_t) size);
  if (js->brk + size > js->size) return ~(jsoff_t) 0;
  js->brk += (jsoff_t) size;
  return ofs;
}

static jsval_t mkentity(struct js *js, jsoff_t b, const void *buf, size_t len) {
  jsoff_t ofs = js_alloc(js, len + sizeof(b));
  if (ofs == (jsoff_t) ~0) return js_mkerr(js, "oom");
  memcpy(&js->mem[ofs], &b, sizeof(b));
  if (buf != NULL) memmove(&js->mem[ofs + sizeof(b)], buf, len);
  if ((b & 3) == T_STR) js->mem[ofs + sizeof(b) + len - 1] = 0;
  return mkval(b & 3, ofs);
}

jsval_t js_mkstr(struct js *js, const void *ptr, size_t len) {
  jsoff_t n = (jsoff_t) (len + 1);
  return mkentity(js, (jsoff_t) ((n << 2) | T_STR), ptr, n);
}

jsval_t js_mkobj(struct js *js) {
  jsoff_t parent = 0;
  return mkentity(js, 0 | T_OBJ, &parent, sizeof(parent));
}

static jsoff_t esize(jsoff_t w) {
  switch (w & 3U) {
    case T_OBJ:   return (jsoff_t) (sizeof(jsoff_t) + sizeof(jsoff_t));
    case T_PROP:  return (jsoff_t) (sizeof(jsoff_t) + sizeof(jsoff_t) + sizeof(jsval_t));
    case T_STR:   return (jsoff_t) (sizeof(jsoff_t) + align32(w >> 2U));
    default:      return (jsoff_t) ~0U;
  }
}

static uint8_t next(struct js *js) {
  if (js->consumed == 0) return js->tok;
  js->consumed = 0;
  js->toff = js->pos = skiptonext(js->code, js->clen, js->pos);
  js->tlen = 0;
  const char *buf = js->code + js->toff;
  if (js->toff >= js->clen) { js->tok = TOK_EOF; return js->tok; }
  switch (buf[0]) {
    case '?': TOK(TOK_Q, 1);
    case ':': TOK(TOK_COLON, 1);
    case '(': TOK(TOK_LPAREN, 1);
    case ')': TOK(TOK_RPAREN, 1);
    case '{': TOK(TOK_LBRACE, 1);
    case '}': TOK(TOK_RBRACE, 1);
    case ';': TOK(TOK_SEMICOLON, 1);
    case ',': TOK(TOK_COMMA, 1);
    case '.': TOK(TOK_DOT, 1);
    case '-': if (LOOK(1, '-')) TOK(TOK_POSTDEC, 2); if (LOOK(1, '=')) TOK(TOK_MINUS_ASSIGN, 2); TOK(TOK_MINUS, 1);
    case '+': if (LOOK(1, '+')) TOK(TOK_POSTINC, 2); if (LOOK(1, '=')) TOK(TOK_PLUS_ASSIGN, 2); TOK(TOK_PLUS, 1);
    case '*': if (LOOK(1, '*')) TOK(TOK_EXP, 2); if (LOOK(1, '=')) TOK(TOK_MUL_ASSIGN, 2); TOK(TOK_MUL, 1);
    case '/': if (LOOK(1, '=')) TOK(TOK_DIV_ASSIGN, 2); TOK(TOK_DIV, 1);
    case '%': if (LOOK(1, '=')) TOK(TOK_REM_ASSIGN, 2); TOK(TOK_REM, 1);
    case '&': if (LOOK(1, '&')) TOK(TOK_LAND, 2); if (LOOK(1, '=')) TOK(TOK_AND_ASSIGN, 2); TOK(TOK_AND, 1);
    case '|': if (LOOK(1, '|')) TOK(TOK_LOR, 2); if (LOOK(1, '=')) TOK(TOK_OR_ASSIGN, 2); TOK(TOK_OR, 1);
    case '=': if (LOOK(1, '=') && LOOK(2, '=')) TOK(TOK_EQ, 3); TOK(TOK_ASSIGN, 1);
    case '<': if (LOOK(1, '<')) TOK(TOK_SHL, 2); if (LOOK(1, '=')) TOK(TOK_LE, 2); TOK(TOK_LT, 1);
    case '>': if (LOOK(1, '>')) TOK(TOK_SHR, 2); if (LOOK(1, '=')) TOK(TOK_GE, 2); TOK(TOK_GT, 1);
    case '!': if (LOOK(1, '=') && LOOK(2, '=')) TOK(TOK_NE, 3); TOK(TOK_NOT, 1);
    case '\"': case '\'': {
      js->tlen++;
      while (js->toff + js->tlen < js->clen && buf[js->tlen] != buf[0]) js->tlen++;
      if (buf[0] == buf[js->tlen]) js->tok = TOK_STRING, js->tlen++;
      break;
    }
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
      char *end;
      js->tval = tov(strtod(buf, &end));
      TOK(TOK_NUMBER, (jsoff_t) (end - buf));
    }
    default: js->tok = parseident(buf, js->clen - js->toff, &js->tlen); break;
  }
  js->pos = js->toff + js->tlen;
  return js->tok;
}

static jsoff_t skiptonext(const char *code, jsoff_t len, jsoff_t n) {
  while (n < len) {
    if (is_space(code[n])) n++;
    else if (n + 1 < len && code[n] == '/' && code[n + 1] == '/') {
      for (n += 2; n < len && code[n] != '\n';) n++;
    } else break;
  }
  return n;
}

static bool streq(const char *buf, size_t len, const char *p, size_t n) { return n == len && memcmp(buf, p, len) == 0; }

static uint8_t parseident(const char *buf, jsoff_t len, jsoff_t *tlen) {
  if (!is_ident_begin(buf[0])) return TOK_ERR;
  while (*tlen < len && is_ident_continue(buf[*tlen])) (*tlen)++;
  return TOK_IDENTIFIER;
}

jsval_t js_eval(struct js *js, const char *code, size_t len) {
  js->code = code;
  js->clen = (jsoff_t) len;
  js->pos = 0;
  js->consumed = 1;
  jsval_t res = js_mkundef();
  while (next(js) != TOK_EOF) {
      if (js->tok == TOK_SEMICOLON) {
          js->consumed = 1;
          continue;
      }
      res = js_expr(js);
  }
  return res;
}

jsval_t js_stmt(struct js *js) {
  return js_expr(js);
}

jsval_t js_expr(struct js *js) {
  jsval_t res = js_mkundef();
  uint8_t t = next(js);
  if (t == TOK_IDENTIFIER) {
      serial_printf("[JS] IDENT: %.*s (len=%d)\n", (int)js->tlen, js->code + js->toff, (int)js->tlen);
      if (streq(js->code + js->toff, js->tlen, "print", 5)) {
          js->consumed = 1;
          if (next(js) == TOK_LPAREN) {
              js->consumed = 1;
              jsval_t arg = js_expr(js);
              if (next(js) == TOK_RPAREN) {
                  js->consumed = 1;
                  jsval_t glob = js_glob(js);
                  jsoff_t off = lkp(js, glob, "print", 5);
                  if (off) {
                      jsval_t fn = loadval(js, (jsoff_t)(off + sizeof(jsoff_t) * 2));
                      serial_printf("[JS] Calling func at off %d\n", (int)vdata(fn));
                      res = do_op(js, TOK_CALL, fn, arg);
                  } else {
                      serial_printf("[JS] print NOT FOUND in global scope\n");
                  }
              }
          }
      }
  } else if (t == TOK_STRING) {
      serial_printf("[JS] STRING: %.*s\n", (int)js->tlen, js->code + js->toff);
      res = js_mkstr(js, js->code + js->toff + 1, js->tlen - 2);
      js->consumed = 1;
  } else {
      js->consumed = 1;
  }
  return res;
}



jsval_t js_mkerr(struct js *js, const char *fmt, ...) {
    strcpy(js->errmsg, "ERROR");
    return mkval(T_ERR, 0);
}

static jsval_t do_op(struct js *js, uint8_t op, jsval_t l, jsval_t r) {
    if (op == TOK_CALL) {
        if (vtype(l) == T_CFUNC) {
            jsoff_t off = (jsoff_t) vdata(l);
            jsval_t (*fn)(struct js *, jsval_t *, int);
            memcpy(&fn, &js->mem[off], sizeof(fn));
            return fn(js, &r, 1);
        }
    }
    return js_mkundef();
}

static jsoff_t lkp(struct js *js, jsval_t obj, const char *buf, size_t len) {
  jsoff_t off = loadoff(js, (jsoff_t) vdata(obj)) & ~3U;
  while (off < js->brk && off != 0) {
    jsoff_t koff = loadoff(js, (jsoff_t) (off + sizeof(off)));
    jsoff_t klen = (loadoff(js, koff) >> 2) - 1;
    const char *p = (char *) &js->mem[koff + sizeof(koff)];
    if (streq(buf, len, p, klen)) return off;
    off = loadoff(js, off) & ~3U;
  }
  return 0;
}

void js_set(struct js *js, jsval_t obj, const char *key, jsval_t val) {
  if (vtype(obj) == T_OBJ) setprop(js, obj, js_mkstr(js, key, strlen(key)), val);
}

static jsval_t setprop(struct js *js, jsval_t obj, jsval_t k, jsval_t v) {
  jsoff_t koff = (jsoff_t) vdata(k);
  jsoff_t b, head = (jsoff_t) vdata(obj);
  char buf[sizeof(koff) + sizeof(v)];
  b = loadoff(js, head);
  memcpy(buf, &koff, sizeof(koff));
  memcpy(buf + sizeof(koff), &v, sizeof(v));
  jsoff_t brk = js->brk | T_OBJ;
  saveoff(js, head, brk);
  return mkentity(js, (b & ~3U) | T_PROP, buf, sizeof(buf));
}

jsval_t js_glob(struct js *js) { return mkval(T_OBJ, 0); }
int js_type(jsval_t val) { return vtype(val); }
const char *js_str(struct js *js, jsval_t val) {
    static char buf[128];
    if (vtype(val) == T_STR) {
        size_t len;
        char *s = js_getstr(js, val, &len);
        return s;
    }
    return "JSVAL";
}

char *js_getstr(struct js *js, jsval_t value, size_t *len) {
  if (vtype(value) != T_STR) return NULL;
  jsoff_t n = offtolen(loadoff(js, (jsoff_t)vdata(value)));
  if (len != NULL) *len = n;
  return (char *) &js->mem[vdata(value) + sizeof(jsoff_t)];
}

jsval_t js_mktrue(void) { return mkval(T_BOOL, 1); }
jsval_t js_mkfalse(void) { return mkval(T_BOOL, 0); }
jsval_t js_mkundef(void) { return mkval(T_UNDEF, 0); }
jsval_t js_mknull(void) { return mkval(T_NULL, 0); }
jsval_t js_mkfun(jsval_t (*fn)(struct js *, jsval_t *, int)) {
    struct js *js = js_ctx_global;
    jsoff_t off = js_alloc(js, sizeof(fn));
    if (off == (jsoff_t) ~0) return js_mkerr(js, "oom");
    memcpy(&js->mem[off], &fn, sizeof(fn));
    return mkval(T_CFUNC, off);
}

void js_setgct(struct js *js, size_t gct) { (void)js; (void)gct; }
void js_setmaxcss(struct js *js, size_t max) { (void)js; (void)max; }

struct js *js_create(void *buf, size_t len) {
  struct js *js = (struct js *) buf;
  memset(buf, 0, len);
  js->mem = (uint8_t *) (js + 1);
  js->size = (jsoff_t) (len - sizeof(*js));
  js->scope = js_mkobj(js);
  js->lwm = js->size;
  js_ctx_global = js;
  return js;
}



