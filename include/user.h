#ifndef SILVEROS_USER_H
#define SILVEROS_USER_H

#include "types.h"

#define MAX_USERS 16
#define MAX_USERNAME 32
#define MAX_PASSWORD_HASH 64

typedef struct {
    int uid;
    char username[MAX_USERNAME];
    char password_hash[MAX_PASSWORD_HASH]; /* Simple hash or plaintext for demo */
} user_t;

void user_init(void);
bool user_login(const char *username, const char *password);
int user_get_current_uid(void);
const char *user_get_current_username(void);
bool user_create(const char *username, const char *password);

#endif
