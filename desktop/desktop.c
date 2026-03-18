#include "../include/desktop.h"
#include "../include/framebuffer.h"
#include "../include/font.h"
#include "../include/mouse.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/string.h"
#include "../include/heap.h"
#include "../include/serial.h"
#include "../include/silverfs.h"
#include "../include/io.h"
#include "../include/rtc.h"
#include "../include/net.h"
#include "../include/rtl8139.h"
#include "../include/v8_runtime.h"
#include "../runtime/libc/stdio.h"

/* ============================================================
 * Color Theme
 * ============================================================ */
#define DESKTOP_BG_TOP     RGB(25, 25, 50)
#define DESKTOP_BG_BOT     RGB(40, 45, 65)
#define TASKBAR_BG         RGB(20, 20, 35)
#define TASKBAR_HEIGHT     32
#define TASKBAR_BORDER     RGB(60, 65, 80)
#define TITLEBAR_BG        RGB(45, 45, 70)
#define TITLEBAR_FOCUSED   RGB(70, 75, 110)
#define TITLEBAR_TEXT      RGB(210, 215, 225)
#define WINDOW_BG          RGB(30, 30, 48)
#define WINDOW_BORDER_COL  RGB(55, 58, 75)
#define CLOSE_BTN_BG       RGB(180, 50, 50)
#define CLOSE_BTN_HOVER    RGB(220, 70, 70)
#define CLOCK_COLOR        RGB(170, 175, 190)
#define MENU_BTN_BG        RGB(55, 60, 85)
#define MENU_BTN_HOVER     RGB(75, 80, 110)
#define MENU_BTN_TEXT      RGB(200, 205, 215)
#define CURSOR_COLOR       RGB(255, 255, 255)
#define CURSOR_OUTLINE     RGB(0, 0, 0)

/* ============================================================
 * Window Manager
 * ============================================================ */
static window_t windows[MAX_WINDOWS];
static int window_count = 0;
static int focused_window = -1;
static int window_order[MAX_WINDOWS]; /* Z-order, back to front */
static int order_count = 0;

int window_create(const char *title, int x, int y, int w, int h) {
    if (window_count >= MAX_WINDOWS) return -1;

    int id = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].visible) { id = i; break; }
    }
    if (id < 0) return -1;

    window_t *win = &windows[id];
    strncpy(win->title, title, 63);
    win->title[63] = 0;
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->visible = true;
    win->focused = false;
    win->dragging = false;
    win->content = NULL;
    win->content_w = w - WINDOW_BORDER * 2;
    win->content_h = h - TITLEBAR_HEIGHT - WINDOW_BORDER;
    win->draw_content = NULL;
    win->handle_key = NULL;
    win->handle_click = NULL;
    win->id = id;

    /* Add to z-order (on top) */
    window_order[order_count++] = id;

    /* Focus this window */
    if (focused_window >= 0) windows[focused_window].focused = false;
    focused_window = id;
    win->focused = true;

    window_count++;
    serial_printf("[WM] Created window '%s' (id=%d, %dx%d at %d,%d)\n",
                  title, (uint64_t)id, (uint64_t)w, (uint64_t)h, (uint64_t)x, (uint64_t)y);
    return id;
}

void window_destroy(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    windows[id].visible = false;
    window_count--;

    /* Remove from z-order */
    for (int i = 0; i < order_count; i++) {
        if (window_order[i] == id) {
            for (int j = i; j < order_count - 1; j++)
                window_order[j] = window_order[j + 1];
            order_count--;
            break;
        }
    }

    /* Update focus */
    if (focused_window == id) {
        focused_window = -1;
        if (order_count > 0) {
            focused_window = window_order[order_count - 1];
            windows[focused_window].focused = true;
        }
    }
}

void window_set_draw_callback(int id, void (*draw)(int)) {
    if (id >= 0 && id < MAX_WINDOWS) windows[id].draw_content = draw;
}

void window_set_key_callback(int id, void (*handler)(int, char)) {
    if (id >= 0 && id < MAX_WINDOWS) windows[id].handle_key = handler;
}

void window_set_click_callback(int id, void (*handler)(int, int, int)) {
    if (id >= 0 && id < MAX_WINDOWS) windows[id].handle_click = handler;
}

window_t *window_get(int id) {
    if (id >= 0 && id < MAX_WINDOWS && windows[id].visible) return &windows[id];
    return NULL;
}

void window_focus(int id) {
    if (id < 0 || id >= MAX_WINDOWS || !windows[id].visible) return;

    /* Remove from z-order */
    for (int i = 0; i < order_count; i++) {
        if (window_order[i] == id) {
            for (int j = i; j < order_count - 1; j++)
                window_order[j] = window_order[j + 1];
            order_count--;
            break;
        }
    }

    /* Add to top of z-order */
    window_order[order_count++] = id;

    /* Update focus state */
    if (focused_window >= 0) windows[focused_window].focused = false;
    focused_window = id;
    windows[id].focused = true;
}

/* ============================================================
 * Drawing
 * ============================================================ */

static void draw_cursor(int mx, int my) {
    /* Simple arrow cursor */
    static const char *cursor[] = {
        "X.",
        "XX.",
        "XXX.",
        "XXXX.",
        "XXXXX.",
        "XXXXXX.",
        "XXXXXXX.",
        "XXXXXXXX.",
        "XXXXXXXXX.",
        "...XXX.",
        "....XXX.",
        ".....XXX.",
        "......XXX.",
        ".......XX.",
        ".....",
    };

    for (int row = 0; row < 15; row++) {
        for (int col = 0; cursor[row][col]; col++) {
            if (cursor[row][col] == 'X') {
                fb_put_pixel(mx + col, my + row, CURSOR_COLOR);
            } else if (cursor[row][col] == '.') {
                fb_put_pixel(mx + col, my + row, CURSOR_OUTLINE);
            }
        }
    }
}

static void draw_window(window_t *win) {
    if (!win->visible) return;

    int x = win->x, y = win->y, w = win->width, h = win->height;

    /* Window shadow */
    fb_fill_rect(x + 4, y + 4, w, h, RGB(5, 5, 15));

    /* Window body & border */
    fb_fill_rect(x, y, w, h, WINDOW_BG);
    fb_draw_rect(x, y, w, h, WINDOW_BORDER_COL);

    /* Title bar */
    uint32_t tb_color = win->focused ? TITLEBAR_FOCUSED : TITLEBAR_BG;
    fb_fill_rect(x + 1, y + 1, w - 2, TITLEBAR_HEIGHT, tb_color);

    /* Title bar gradient (top lighter) */
    for (int i = 0; i < TITLEBAR_HEIGHT / 2; i++) {
        for (int j = 1; j < w - 1; j++) {
            uint32_t c = fb_get_pixel(x + j, y + 1 + i);
            fb_put_pixel(x + j, y + 1 + i, fb_blend(c, RGB(255, 255, 255), 15 - i));
        }
    }

    /* Title text */
    font_draw_string(x + 8, y + 5, win->title, TITLEBAR_TEXT);

    /* Close button */
    int cbx = x + w - CLOSE_BTN_SIZE - 5;
    int cby = y + 4;
    fb_fill_rect(cbx, cby, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE, CLOSE_BTN_BG);
    /* X in close button */
    for (int i = 3; i < CLOSE_BTN_SIZE - 3; i++) {
        fb_put_pixel(cbx + i, cby + i, CURSOR_COLOR);
        fb_put_pixel(cbx + CLOSE_BTN_SIZE - 1 - i, cby + i, CURSOR_COLOR);
    }

    /* Content area */
    if (win->draw_content) {
        win->draw_content(win->id);
    }
}

static void draw_taskbar(void) {
    int tb_y = fb.height - TASKBAR_HEIGHT;

    /* Taskbar background */
    fb_fill_rect(0, tb_y, fb.width, TASKBAR_HEIGHT, TASKBAR_BG);
    fb_draw_line(0, tb_y, fb.width, tb_y, TASKBAR_BORDER);

    /* "Silver" menu button */
    fb_fill_rect(4, tb_y + 4, 80, TASKBAR_HEIGHT - 8, MENU_BTN_BG);
    fb_draw_rect(4, tb_y + 4, 80, TASKBAR_HEIGHT - 8, TASKBAR_BORDER);
    font_draw_string(14, tb_y + 8, "Silver", MENU_BTN_TEXT);

    /* Window buttons in taskbar */
    int btn_x = 100;
    for (int i = 0; i < order_count; i++) {
        window_t *win = &windows[window_order[i]];
        if (!win->visible) continue;

        uint32_t bg = win->focused ? TITLEBAR_FOCUSED : MENU_BTN_BG;
        fb_fill_rect(btn_x, tb_y + 4, 120, TASKBAR_HEIGHT - 8, bg);
        fb_draw_rect(btn_x, tb_y + 4, 120, TASKBAR_HEIGHT - 8, TASKBAR_BORDER);

        char short_title[14];
        strncpy(short_title, win->title, 13);
        short_title[13] = 0;
        font_draw_string(btn_x + 6, tb_y + 8, short_title, TITLEBAR_TEXT);
        btn_x += 126;
    }

    /* Clock on the right */
    rtc_time_t t;
    rtc_get_time(&t);
    char clock[16];
    snprintf(clock, sizeof(clock), "%02d:%02d", (uint64_t)t.hour, (uint64_t)t.minute);
    font_draw_string(fb.width - 60, tb_y + 8, clock, CLOCK_COLOR);
}

static void draw_desktop_bg(void) {
    fb_draw_gradient_v(0, 0, fb.width, fb.height - TASKBAR_HEIGHT, DESKTOP_BG_TOP, DESKTOP_BG_BOT);

    /* "SilverOS" watermark in bottom-right */
    font_draw_string(fb.width - 140, fb.height - TASKBAR_HEIGHT - 25,
                     "SilverOS v0.1", RGB(50, 55, 70));
}

/* ============================================================
 * Terminal App
 * ============================================================ */

#define TERM_MAX_LINES 256
#define TERM_MAX_COLS  80
#define TERM_INPUT_MAX 256

static char term_lines[TERM_MAX_LINES][TERM_MAX_COLS + 1];
static int  term_line_count = 0;
static char term_input[TERM_INPUT_MAX];
static int  term_input_len = 0;
static int  term_window_id = -1;
#if ENABLE_V8
static void *term_v8_runtime = NULL;
#endif

static void term_add_line(const char *line) {
    if (term_line_count < TERM_MAX_LINES) {
        strncpy(term_lines[term_line_count], line, TERM_MAX_COLS);
        term_lines[term_line_count][TERM_MAX_COLS] = 0;
        term_line_count++;
    } else {
        /* Scroll up */
        for (int i = 0; i < TERM_MAX_LINES - 1; i++)
            strcpy(term_lines[i], term_lines[i + 1]);
        strncpy(term_lines[TERM_MAX_LINES - 1], line, TERM_MAX_COLS);
    }
}

static void term_show_webapp_info(void) {
    term_add_line("Renderer: WebKit + JavaScriptCore");
    term_add_line("Command: webapp");
    term_add_line("Entry: /home/user/webapp/index.html");
    term_add_line("React src: /home/user/webapp/src/main.jsx");
    term_add_line("Bundle: /home/user/webapp/app.js");
#if ENABLE_V8
    term_add_line("Native: /home/user/webapp/native.js (V8 enabled)");
#else
    term_add_line("Native: /home/user/webapp/native.js (V8 disabled)");
#endif
}

#if ENABLE_V8
static bool term_ensure_native_runtime(void) {
    if (term_v8_runtime) {
        return true;
    }

    term_v8_runtime = create_v8_runtime();
    return term_v8_runtime != NULL;
}

static void term_run_native_script(const char *path) {
    silverfs_inode_t inode;
    silverfs_file_t file;
    char *source = NULL;
    char line[TERM_MAX_COLS + 1];
    int bytes_read = 0;

    if (!path || !path[0]) {
        path = SILVEROS_WEB_APP_NATIVE_SCRIPT_PATH;
    }

    if (silverfs_stat(path, &inode) < 0) {
        snprintf(line, sizeof(line), "native: file not found: %s", path);
        term_add_line(line);
        return;
    }

    if (inode.type != SILVERFS_TYPE_FILE) {
        snprintf(line, sizeof(line), "native: not a file: %s", path);
        term_add_line(line);
        return;
    }

    source = (char *)kmalloc((size_t)inode.size + 1);
    if (!source) {
        term_add_line("native: out of memory");
        return;
    }

    if (silverfs_open(path, &file) < 0) {
        kfree(source);
        snprintf(line, sizeof(line), "native: failed to open: %s", path);
        term_add_line(line);
        return;
    }

    bytes_read = silverfs_read(&file, source, inode.size);
    silverfs_close(&file);
    if (bytes_read < 0) {
        kfree(source);
        snprintf(line, sizeof(line), "native: failed to read: %s", path);
        term_add_line(line);
        return;
    }

    source[bytes_read] = '\0';

    if (!term_ensure_native_runtime()) {
        kfree(source);
        term_add_line("native: failed to initialize V8 runtime");
        return;
    }

    if (v8_execute_script(term_v8_runtime, source, path)) {
        snprintf(line, sizeof(line), "native: executed %s", path);
    } else {
        snprintf(line, sizeof(line), "native: execution failed: %s", path);
    }

    kfree(source);
    term_add_line(line);
}
#else
static void term_run_native_script(const char *path) {
    (void)path;
    term_add_line("native: V8 runtime is not enabled in this build");
}
#endif

static void term_process_command(const char *cmd) {
    if (strlen(cmd) == 0) {
        term_add_line("");
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        term_add_line("SilverOS Terminal - Available commands:");
        term_add_line("  help     - Show this message");
        term_add_line("  clear    - Clear terminal");
        term_add_line("  ls [dir] - List directory");
        term_add_line("  cat file - Show file contents");
        term_add_line("  echo txt - Print text");
        term_add_line("  mkdir d  - Create directory");
        term_add_line("  touch f  - Create file");
        term_add_line("  write f  - Write to file");
        term_add_line("  webapp   - Open the seeded Web App");
        term_add_line("  webinfo  - Show WebKit/React/V8 paths");
        term_add_line("  native f - Run a native V8 script");
        term_add_line("  uname    - System information");
        term_add_line("  free     - Memory information");
        term_add_line("  date     - Show hardware date/time");
        term_add_line("  ifconfig - Show network interfaces");
        term_add_line("  ping ip  - Send ICMP echo request");
    } else if (strcmp(cmd, "clear") == 0) {
        term_line_count = 0;
    } else if (strcmp(cmd, "uname") == 0) {
        term_add_line("SilverOS v0.1.0 x86_64");
    } else if (strcmp(cmd, "free") == 0) {
        char buf[80];
        extern uint64_t pmm_get_free_pages(void);
        uint64_t free = pmm_get_free_pages();
        snprintf(buf, sizeof(buf), "Free pages: %d (%d MB)", free, (free * 4096) / (1024 * 1024));
        term_add_line(buf);
    } else if (strcmp(cmd, "date") == 0) {
        char buf[80];
        rtc_time_t t;
        rtc_get_time(&t);
        snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d", 
                 t.year, t.month, t.day, t.hour, t.minute, t.second);
        term_add_line(buf);
    } else if (strcmp(cmd, "ifconfig") == 0) {
        char buf[80];
        snprintf(buf, sizeof(buf), "eth0: MAC %02x:%02x:%02x:%02x:%02x:%02x", 
                 rtl8139_mac[0], rtl8139_mac[1], rtl8139_mac[2],
                 rtl8139_mac[3], rtl8139_mac[4], rtl8139_mac[5]);
        term_add_line(buf);
        snprintf(buf, sizeof(buf), "      IP  %d.%d.%d.%d", 
                 net_my_ip[0], net_my_ip[1], net_my_ip[2], net_my_ip[3]);
        term_add_line(buf);
    } else if (strncmp(cmd, "ping ", 5) == 0) {
        int ip_arr[4] = {0};
        const char *p = cmd + 5;
        int i=0, err = 0;
        while (*p && i < 4) {
            if (*p >= '0' && *p <= '9') {
                ip_arr[i] = ip_arr[i] * 10 + (*p - '0');
            } else if (*p == '.') {
                i++;
            } else {
                err = 1; break;
            }
            p++;
        }
        if (i != 3 || err) {
            term_add_line("ping: invalid IP address format");
        } else {
            ip_addr_t target = {ip_arr[0], ip_arr[1], ip_arr[2], ip_arr[3]};
            icmp_send_echo_request(target);
            term_add_line("Ping request sent (check serial for replies)");
        }
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_add_line(cmd + 5);
    } else if (strcmp(cmd, "webapp") == 0) {
        web_app_open(SILVEROS_WEB_APP_URL);
        term_add_line("Opened SilverOS Web App");
    } else if (strcmp(cmd, "webinfo") == 0) {
        term_show_webapp_info();
    } else if (strcmp(cmd, "native") == 0) {
        term_run_native_script(SILVEROS_WEB_APP_NATIVE_SCRIPT_PATH);
    } else if (strncmp(cmd, "native ", 7) == 0) {
        term_run_native_script(cmd + 7);
    } else if (strcmp(cmd, "ls") == 0 || strncmp(cmd, "ls ", 3) == 0) {
        const char *path = "/";
        if (strncmp(cmd, "ls ", 3) == 0 && strlen(cmd) > 3) path = cmd + 3;

        silverfs_dirent_t entries[32];
        int count = silverfs_readdir(path, entries, 32);
        if (count < 0) {
            term_add_line("ls: directory not found");
        } else if (count == 0) {
            term_add_line("(empty)");
        } else {
            for (int i = 0; i < count; i++) {
                char line[80];
                silverfs_inode_t inode;
                char entry_path[512];
                if (strcmp(path, "/") == 0) {
                    snprintf(entry_path, sizeof(entry_path), "/%s", entries[i].name);
                } else {
                    snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entries[i].name);
                }
                silverfs_stat(entry_path, &inode);
                if (inode.type == SILVERFS_TYPE_DIR) {
                    snprintf(line, sizeof(line), "  [DIR]  %s", entries[i].name);
                } else {
                    snprintf(line, sizeof(line), "  %6d %s", (uint64_t)inode.size, entries[i].name);
                }
                term_add_line(line);
            }
        }
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        const char *path = cmd + 4;
        silverfs_file_t file;
        if (silverfs_open(path, &file) < 0) {
            term_add_line("cat: file not found");
        } else {
            char buf[256];
            int n = silverfs_read(&file, buf, 255);
            buf[n] = 0;
            silverfs_close(&file);
            term_add_line(buf);
        }
    } else if (strncmp(cmd, "mkdir ", 6) == 0) {
        if (silverfs_mkdir(cmd + 6) < 0) {
            term_add_line("mkdir: failed");
        } else {
            term_add_line("Directory created");
        }
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        if (silverfs_create(cmd + 6, SILVERFS_TYPE_FILE) < 0) {
            term_add_line("touch: failed");
        } else {
            term_add_line("File created");
        }
    } else if (strncmp(cmd, "write ", 6) == 0) {
        /* write /path data... */
        const char *rest = cmd + 6;
        const char *space = strchr(rest, ' ');
        if (!space) {
            term_add_line("Usage: write /path data");
        } else {
            char path[256];
            int plen = space - rest;
            strncpy(path, rest, plen);
            path[plen] = 0;

            silverfs_file_t file;
            if (silverfs_open(path, &file) < 0) {
                term_add_line("write: file not found");
            } else {
                const char *data = space + 1;
                silverfs_write(&file, data, strlen(data));
                silverfs_close(&file);
                term_add_line("Written successfully");
            }
        }
    } else {
        char buf[80];
        snprintf(buf, sizeof(buf), "Unknown command: %s", cmd);
        term_add_line(buf);
    }
}

static void term_draw(int id) {
    window_t *win = window_get(id);
    if (!win) return;

    int cx = win->x + WINDOW_BORDER;
    int cy = win->y + TITLEBAR_HEIGHT + 2;
    int cw = win->width - WINDOW_BORDER * 2;
    int ch = win->height - TITLEBAR_HEIGHT - WINDOW_BORDER - 2;

    /* Terminal background */
    fb_fill_rect(cx, cy, cw, ch, RGB(15, 15, 25));

    /* Draw lines */
    int visible_lines = ch / FONT_HEIGHT;
    int start = term_line_count > visible_lines ? term_line_count - visible_lines : 0;

    for (int i = start; i < term_line_count; i++) {
        int row = i - start;
        font_draw_string(cx + 4, cy + row * FONT_HEIGHT, term_lines[i], RGB(180, 190, 200));
    }

    /* Draw input line */
    int input_y = cy + (term_line_count - start) * FONT_HEIGHT;
    if (input_y + FONT_HEIGHT < cy + ch) {
        font_draw_string(cx + 4, input_y, "$ ", RGB(100, 200, 120));
        font_draw_string(cx + 4 + FONT_WIDTH * 2, input_y, term_input, RGB(210, 215, 225));

        /* Blinking cursor */
        if ((timer_get_ticks() / 500) % 2 == 0) {
            int cursor_x = cx + 4 + FONT_WIDTH * (2 + term_input_len);
            fb_fill_rect(cursor_x, input_y, FONT_WIDTH, FONT_HEIGHT, RGB(180, 190, 200));
        }
    }
}

static void term_key_handler(int id, char key) {
    (void)id;
    if (key == KEY_ENTER) {
        char prompt_line[TERM_MAX_COLS + 1];
        snprintf(prompt_line, sizeof(prompt_line), "$ %s", term_input);
        term_add_line(prompt_line);
        term_process_command(term_input);
        term_input[0] = 0;
        term_input_len = 0;
    } else if (key == KEY_BACKSPACE || key == 0x7F) {
        if (term_input_len > 0) {
            term_input[--term_input_len] = 0;
        }
    } else if (key >= 0x20 && key < 0x7F) {
        if (term_input_len < TERM_INPUT_MAX - 1) {
            term_input[term_input_len++] = key;
            term_input[term_input_len] = 0;
        }
    }
}

void terminal_open(void) {
    if (term_window_id >= 0 && window_get(term_window_id)) {
        window_focus(term_window_id);
        return;
    }

    term_line_count = 0;
    term_input_len = 0;
    term_input[0] = 0;

    term_add_line("  ___ _ _             ___  ___");
    term_add_line(" / __(_) |_ _____ _ _/ _ \\/ __|");
    term_add_line(" \\__ \\ | \\ V / -_) '_| (_) \\__ \\");
    term_add_line(" |___/_|_|\\_/\\___|_|  \\___/|___/");
    term_add_line("");
    term_add_line("Welcome to SilverOS Terminal");
    term_add_line("Type 'help' for available commands.");
    term_add_line("");

    term_window_id = window_create("Terminal", 100, 80, 640, 440);
    if (term_window_id >= 0) {
        window_set_draw_callback(term_window_id, term_draw);
        window_set_key_callback(term_window_id, term_key_handler);
    }
}

/* ============================================================
 * Desktop Shell
 * ============================================================ */

static bool menu_open = false;

static void draw_menu(void) {
    if (!menu_open) return;

    int mx = 4;
    int my = fb.height - TASKBAR_HEIGHT - 130;
    int mw = 160;
    int mh = 126;

    /* Menu background */
    fb_fill_rect(mx, my, mw, mh, RGB(35, 35, 55));
    fb_draw_rect(mx, my, mw, mh, TASKBAR_BORDER);

    /* Menu items */
    const char *items[] = {"File Browser", "Terminal", "Web App", "About", "---", "Shutdown"};
    int item_count = 6;

    for (int i = 0; i < item_count; i++) {
        int iy = my + 6 + i * 28;
        if (strcmp(items[i], "---") == 0) {
            fb_draw_line(mx + 8, iy + 12, mx + mw - 8, iy + 12, TASKBAR_BORDER);
        } else {
            font_draw_string(mx + 12, iy + 6, items[i], MENU_BTN_TEXT);
        }
    }
}

static void handle_menu_click(int mx, int my) {
    int menu_x = 4;
    int menu_y = fb.height - TASKBAR_HEIGHT - 130;

    if (mx >= menu_x && mx < menu_x + 160 && my >= menu_y && my < menu_y + 126) {
        int item = (my - menu_y - 6) / 28;
        switch (item) {
            case 0: /* File Browser */
                file_browser_open("/");
                menu_open = false;
                break;
            case 1: /* Terminal */
                terminal_open();
                menu_open = false;
                break;
            case 2: /* Web App */
                web_app_open(SILVEROS_WEB_APP_URL);
                menu_open = false;
                break;
            case 3: /* About */
                {
                    int about_id = window_create("About SilverOS", 250, 200, 400, 250);
                    if (about_id >= 0) {
                        /* Simple about draw callback */
                        window_set_draw_callback(about_id, NULL);
                    }
                }
                menu_open = false;
                break;
            case 5: /* Shutdown */
                serial_printf("[DESKTOP] Shutdown requested\n");
                /* In QEMU, write to ACPI poweroff port */
                outb(0x604, 0x2000 & 0xFF);
                break;
        }
    } else {
        menu_open = false;
    }
}

#include "../include/user.h"

static bool desktop_logged_in = false;
static int login_window_id = -1;
static char login_username[32] = "";
static char login_password[64] = "";
static int login_focus_field = 0; /* 0 = username, 1 = password */

static void login_draw(int id) {
    window_t *win = window_get(id);
    if (!win) return;

    int cx = win->x + WINDOW_BORDER;
    int cy = win->y + TITLEBAR_HEIGHT + 2;

    fb_fill_rect(cx, cy, win->content_w, win->content_h, RGB(30, 30, 48));

    font_draw_string(cx + 60, cy + 20, "SilverOS Login", RGB(220, 220, 240));

    font_draw_string(cx + 20, cy + 60, "Username:", RGB(200, 200, 200));
    fb_fill_rect(cx + 100, cy + 55, 180, 20, login_focus_field == 0 ? RGB(50, 50, 70) : RGB(20, 20, 30));
    fb_draw_rect(cx + 100, cy + 55, 180, 20, RGB(100, 100, 120));
    font_draw_string(cx + 105, cy + 60, login_username, RGB(255, 255, 255));

    font_draw_string(cx + 20, cy + 90, "Password:", RGB(200, 200, 200));
    fb_fill_rect(cx + 100, cy + 85, 180, 20, login_focus_field == 1 ? RGB(50, 50, 70) : RGB(20, 20, 30));
    fb_draw_rect(cx + 100, cy + 85, 180, 20, RGB(100, 100, 120));
    
    char masked[64];
    int plen = strlen(login_password);
    for (int i = 0; i < plen; i++) masked[i] = '*';
    masked[plen] = '\0';
    font_draw_string(cx + 105, cy + 90, masked, RGB(255, 255, 255));

    fb_fill_rect(cx + 120, cy + 130, 80, 24, RGB(55, 60, 85));
    fb_draw_rect(cx + 120, cy + 130, 80, 24, RGB(100, 100, 120));
    font_draw_string(cx + 135, cy + 135, "Login", RGB(220, 220, 240));
}

static void login_key_handler(int id, char key) {
    (void)id;
    if (key == KEY_TAB) {
        login_focus_field = 1 - login_focus_field;
        return;
    }

    if (key == KEY_ENTER) {
        if (user_login(login_username, login_password)) {
            desktop_logged_in = true;
            window_destroy(login_window_id);
            login_window_id = -1;
            /* Launch default desktop apps */
            terminal_open();
        } else {
            login_password[0] = '\0'; /* Clear password on fail */
        }
        return;
    }

    if (key == KEY_BACKSPACE || key == 0x7F) {
        if (login_focus_field == 0 && strlen(login_username) > 0) {
            login_username[strlen(login_username) - 1] = '\0';
        } else if (login_focus_field == 1 && strlen(login_password) > 0) {
            login_password[strlen(login_password) - 1] = '\0';
        }
    } else if (key >= 0x20 && key < 0x7F) {
        if (login_focus_field == 0 && strlen(login_username) < 31) {
            int len = strlen(login_username);
            login_username[len] = key;
            login_username[len + 1] = '\0';
        } else if (login_focus_field == 1 && strlen(login_password) < 63) {
            int len = strlen(login_password);
            login_password[len] = key;
            login_password[len + 1] = '\0';
        }
    }
}

static void login_click_handler(int id, int mx, int my) {
    window_t *win = window_get(id);
    if (!win) return;
    
    int cx = win->x + WINDOW_BORDER;
    int cy = win->y + TITLEBAR_HEIGHT + 2;
    
    int local_x = mx - cx;
    int local_y = my - cy;
    
    if (local_y >= 55 && local_y <= 75 && local_x >= 100 && local_x <= 280) {
        login_focus_field = 0;
    } else if (local_y >= 85 && local_y <= 105 && local_x >= 100 && local_x <= 280) {
        login_focus_field = 1;
    } else if (local_y >= 130 && local_y <= 154 && local_x >= 120 && local_x <= 200) {
        /* Clicked Login button */
        login_key_handler(id, KEY_ENTER);
    }
}

static void ensure_file_with_contents(const char *path, const char *data) {
    silverfs_file_t file;

    if (silverfs_open(path, &file) >= 0) {
        silverfs_close(&file);
        return;
    }

    if (silverfs_create(path, SILVERFS_TYPE_FILE) < 0) {
        return;
    }

    if (silverfs_open(path, &file) >= 0) {
        silverfs_write(&file, data, strlen(data));
        silverfs_close(&file);
    }
}

void desktop_init(void) {
    memset(windows, 0, sizeof(windows));
    window_count = 0;
    focused_window = -1;
    order_count = 0;
    menu_open = false;
    desktop_logged_in = false;

    /* Create some default files in SilverFS */
    silverfs_mkdir("/home");
    silverfs_mkdir("/home/user");
    silverfs_mkdir("/home/user/webapp");
    silverfs_mkdir("/home/user/webapp/src");
    silverfs_mkdir("/etc");
    silverfs_mkdir("/tmp");

    ensure_file_with_contents("/etc/hostname", "silveros");
    ensure_file_with_contents("/home/user/readme.txt",
                              "Welcome to SilverOS!\n"
                              "This is your home directory.\n"
                              "Type 'help' in the terminal for commands.");
    ensure_file_with_contents(SILVEROS_WEB_APP_INDEX_PATH,
                              "<!doctype html>\n"
                              "<html>\n"
                              "<head>\n"
                              "  <meta charset=\"utf-8\">\n"
                              "  <title>SilverOS Web App</title>\n"
                              "  <style>\n"
                              "    :root { color-scheme: dark; }\n"
                              "    body {\n"
                              "      margin: 0;\n"
                              "      min-height: 100vh;\n"
                              "      display: grid;\n"
                              "      place-items: center;\n"
                              "      font-family: sans-serif;\n"
                              "      background: linear-gradient(160deg, #0c1325, #17304b 60%, #274d67);\n"
                              "      color: #f4f7fb;\n"
                              "    }\n"
                              "    .card {\n"
                              "      width: min(680px, calc(100vw - 48px));\n"
                              "      padding: 32px;\n"
                              "      border-radius: 24px;\n"
                              "      background: rgba(10, 16, 28, 0.8);\n"
                              "      border: 1px solid rgba(143, 188, 255, 0.25);\n"
                              "      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.35);\n"
                              "    }\n"
                              "    .eyebrow {\n"
                              "      display: inline-block;\n"
                              "      margin-bottom: 12px;\n"
                              "      padding: 4px 10px;\n"
                              "      border-radius: 999px;\n"
                              "      background: rgba(110, 182, 255, 0.16);\n"
                              "      color: #b7d8ff;\n"
                              "      font-size: 12px;\n"
                              "      letter-spacing: 0.08em;\n"
                              "      text-transform: uppercase;\n"
                              "    }\n"
                              "    h1 { margin: 0 0 12px; font-size: 40px; }\n"
                              "    p { line-height: 1.6; color: #d5ddea; }\n"
                              "    code {\n"
                              "      padding: 2px 8px;\n"
                              "      border-radius: 999px;\n"
                              "      background: rgba(128, 176, 255, 0.14);\n"
                              "    }\n"
                              "  </style>\n"
                              "</head>\n"
                              "<body>\n"
                              "  <div id=\"root\"></div>\n"
                              "  <script src=\"app.js\"></script>\n"
                              "</body>\n"
                              "</html>\n");
    ensure_file_with_contents(SILVEROS_WEB_APP_BUNDLE_PATH,
                              "(function () {\n"
                              "  var root = document.getElementById('root');\n"
                              "  if (!root) return;\n"
                              "  var now = new Date().toString();\n"
                              "  root.innerHTML = '' +\n"
                              "    '<section class=\"card\">' +\n"
                              "    '<span class=\"eyebrow\">WebKit + JSC</span>' +\n"
                              "    '<h1>SilverOS Web App</h1>' +\n"
                              "    '<p>This is the seeded web-app entry SilverOS will hand to the WebKit renderer.</p>' +\n"
                              "    '<p>Page JavaScript runs on JavaScriptCore inside WebKit. Replace <code>app.js</code> with your bundled React output.</p>' +\n"
                              "    '<p>Your React source entry lives at <code>/home/user/webapp/src/main.jsx</code>.</p>' +\n"
                              "    '<p>The native companion script is <code>/home/user/webapp/native.js</code> and is reserved for the separate V8 runtime.</p>' +\n"
                              "    '<p>Boot timestamp: <code>' + now + '</code></p>' +\n"
                              "    '</section>';\n"
                              "}());\n");
    ensure_file_with_contents(SILVEROS_WEB_APP_REACT_ENTRY_PATH,
                              "/*\n"
                              " * SilverOS React entry point.\n"
                              " * Bundle this file into /home/user/webapp/app.js for the WebKit renderer.\n"
                              " */\n"
                              "\n"
                              "const App = () => React.createElement(\n"
                              "  'section',\n"
                              "  { className: 'card' },\n"
                              "  React.createElement('span', { className: 'eyebrow' }, 'React + WebKit'),\n"
                              "  React.createElement('h1', null, 'SilverOS React App'),\n"
                              "  React.createElement(\n"
                              "    'p',\n"
                              "    null,\n"
                              "    'This UI runs inside WebKit on JavaScriptCore. Native OS scripting stays in V8.'\n"
                              "  )\n"
                              ");\n"
                              "\n"
                              "const root = document.getElementById('root');\n"
                              "if (root && window.ReactDOM && ReactDOM.createRoot) {\n"
                              "  ReactDOM.createRoot(root).render(React.createElement(App));\n"
                              "}\n");
    ensure_file_with_contents(SILVEROS_WEB_APP_NATIVE_SCRIPT_PATH,
                              "println('SilverOS native web app companion');\n"
                              "println('This script is meant for the separate V8 runtime, not the WebKit page runtime.');\n"
                              "println(os.version());\n");
    ensure_file_with_contents("/home/user/webapp/README.txt",
                              "SilverOS Web App Layout\n"
                              "=======================\n"
                              "\n"
                              "Renderer path:\n"
                              "  /home/user/webapp/index.html\n"
                              "\n"
                              "Page JavaScript engine:\n"
                              "  JavaScriptCore inside WebKit\n"
                              "\n"
                              "React source entry:\n"
                              "  /home/user/webapp/src/main.jsx\n"
                              "\n"
                              "Bundled page output:\n"
                              "  /home/user/webapp/app.js\n"
                              "\n"
                              "Native OS script entry:\n"
                              "  /home/user/webapp/native.js\n"
                              "\n"
                              "Desktop terminal commands:\n"
                              "  webapp, webinfo, native [file]\n"
                              "\n"
                              "Use V8 for native SilverOS scripting and WebKit/JSC for DOM rendering.\n");
    ensure_file_with_contents("/home/user/index.html",
                              "<!doctype html>\n"
                              "<meta charset=\"utf-8\">\n"
                              "<title>SilverOS Compatibility Page</title>\n"
                              "<body style=\"font-family:sans-serif;background:#0c1325;color:#f4f7fb;padding:32px\">\n"
                              "  <h1>SilverOS Web App moved</h1>\n"
                              "  <p>Open <code>file:///home/user/webapp/index.html</code> for the WebKit-targeted app entry.</p>\n"
                              "</body>\n");

    serial_printf("[DESKTOP] Initializing Login Screen...\n");

    /* Show login window centered */
    login_window_id = window_create("Login", (fb.width - 320) / 2, (fb.height - 200) / 2, 320, 200);
    if (login_window_id >= 0) {
        window_set_draw_callback(login_window_id, login_draw);
        window_set_key_callback(login_window_id, login_key_handler);
        window_set_click_callback(login_window_id, login_click_handler);
    }
}

void desktop_run(void) {
    serial_printf("[DESKTOP] Entering main event loop\n");

    while (1) {
        /* --- Draw --- */
        draw_desktop_bg();

        /* Draw windows (back to front) */
        for (int i = 0; i < order_count; i++) {
            draw_window(&windows[window_order[i]]);
        }

        draw_taskbar();
        draw_menu();

        /* Draw cursor last (on top of everything) */
        mouse_state_t ms;
        mouse_get_state(&ms);
        draw_cursor(ms.x, ms.y);

        fb_swap_buffers();

        /* --- Handle input --- */
        mouse_get_state(&ms);

        if (ms.left_clicked) {
            mouse_clear_clicks();

            int tb_y = fb.height - TASKBAR_HEIGHT;

            /* Check taskbar clicks */
            if (ms.y >= tb_y) {
                /* Silver button */
                if (ms.x >= 4 && ms.x < 84 && ms.y >= tb_y + 4) {
                    menu_open = !menu_open;
                } else {
                    /* Task buttons */
                    int btn_x = 100;
                    for (int i = 0; i < order_count; i++) {
                        if (ms.x >= btn_x && ms.x < btn_x + 120) {
                            window_focus(window_order[i]);
                            break;
                        }
                        btn_x += 126;
                    }
                }
            }
            /* Check menu */
            else if (menu_open) {
                handle_menu_click(ms.x, ms.y);
            }
            /* Check windows (top to bottom) */
            else {
                bool hit = false;
                for (int i = order_count - 1; i >= 0; i--) {
                    window_t *win = &windows[window_order[i]];
                    if (!win->visible) continue;

                    if (ms.x >= win->x && ms.x < win->x + win->width &&
                        ms.y >= win->y && ms.y < win->y + win->height) {

                        /* Close button? */
                        int cbx = win->x + win->width - CLOSE_BTN_SIZE - 5;
                        int cby = win->y + 4;
                        if (ms.x >= cbx && ms.x < cbx + CLOSE_BTN_SIZE &&
                            ms.y >= cby && ms.y < cby + CLOSE_BTN_SIZE) {
                            if (win->id == term_window_id) term_window_id = -1;
                            window_destroy(win->id);
                            hit = true;
                            break;
                        }

                        /* Title bar? → start drag */
                        if (ms.y < win->y + TITLEBAR_HEIGHT) {
                            win->dragging = true;
                            win->drag_offset_x = ms.x - win->x;
                            win->drag_offset_y = ms.y - win->y;
                        } else if (win->handle_click) {
                            /* Content area click */
                            win->handle_click(win->id, ms.x, ms.y);
                        }

                        window_focus(win->id);
                        hit = true;
                        break;
                    }
                }
                if (!hit) {
                    menu_open = false;
                }
            }
        }

        if (ms.left_released) {
            mouse_clear_clicks();
            /* Stop dragging all windows */
            for (int i = 0; i < MAX_WINDOWS; i++) {
                windows[i].dragging = false;
            }
        }

        /* Dragging */
        if (ms.left_button) {
            for (int i = 0; i < MAX_WINDOWS; i++) {
                if (windows[i].dragging) {
                    windows[i].x = ms.x - windows[i].drag_offset_x;
                    windows[i].y = ms.y - windows[i].drag_offset_y;
                    /* Clamp so you can't lose the window */
                    if (windows[i].y < 0) windows[i].y = 0;
                    if (windows[i].y > (int)fb.height - TASKBAR_HEIGHT - 10)
                        windows[i].y = fb.height - TASKBAR_HEIGHT - 10;
                }
            }
        }

        /* Keyboard input → focused window */
        char key = keyboard_getchar_nb();
        if (key && focused_window >= 0 && windows[focused_window].handle_key) {
            windows[focused_window].handle_key(focused_window, key);
        }

        /* Small delay to not burn CPU (will be replaced by proper scheduling) */
        sleep_ms(16);
    }
}
