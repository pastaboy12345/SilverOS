#include "../include/user.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/silverfs.h"

static user_t users[MAX_USERS];
static int num_users = 0;
static int current_uid = -1;

/* In a real OS, passwords should be securely hashed. 
 * For this demo, we use a simple plaintext comparison or basic XOR hash. */

void user_init(void) {
    /* Try to open /etc/passwd from SilverFS. If it doesn't exist, create default user 'root' */
    silverfs_file_t file;
    
    /* Ensure /etc exists */
    silverfs_mkdir("/etc");
    
    if (silverfs_open("/etc/passwd", &file) != 0) {
        serial_printf("[USER] /etc/passwd not found. Creating default 'root' user.\n");
        user_create("root", "silver");
    } else {
        serial_printf("[USER] Loading users from /etc/passwd...\n");
        char buf[512];
        int bytes = silverfs_read(&file, buf, sizeof(buf) - 1);
        silverfs_close(&file);
        
        if (bytes > 0) {
            buf[bytes] = '\0';
            num_users = 0;
            
            char *ptr = buf;
            while (*ptr && num_users < MAX_USERS) {
                /* Format: uid:username:password_hash\n */
                int uid;
                char username[MAX_USERNAME];
                char pass_hash[MAX_PASSWORD_HASH];
                
                char *colon1 = strchr(ptr, ':');
                if (!colon1) break;
                
                char *colon2 = strchr(colon1 + 1, ':');
                if (!colon2) break;
                
                char *newline = strchr(colon2 + 1, '\n');
                
                *colon1 = '\0';
                *colon2 = '\0';
                if (newline) *newline = '\0';
                
                /* Simple atoi */
                uid = 0;
                for (char *c = ptr; *c; c++) uid = uid * 10 + (*c - '0');
                
                strncpy(username, colon1 + 1, MAX_USERNAME - 1);
                
                if (newline) {
                    strncpy(pass_hash, colon2 + 1, MAX_PASSWORD_HASH - 1);
                    ptr = newline + 1;
                } else {
                    strncpy(pass_hash, colon2 + 1, MAX_PASSWORD_HASH - 1);
                    ptr = colon2 + strlen(colon2 + 1); /* end of string */
                }
                
                users[num_users].uid = uid;
                strcpy(users[num_users].username, username);
                strcpy(users[num_users].password_hash, pass_hash);
                num_users++;
            }
        }
    }
}

static void save_users(void) {
    /* Delete existing file and recreate to avoid overwrite sizing issues in basic SilverFS */
    silverfs_delete("/etc/passwd");
    silverfs_create("/etc/passwd", SILVERFS_TYPE_FILE);
    
    silverfs_file_t file;
    if (silverfs_open("/etc/passwd", &file) == 0) {
        char buf[128];
        for (int i = 0; i < num_users; i++) {
            snprintf(buf, sizeof(buf), "%d:%s:%s\n", users[i].uid, users[i].username, users[i].password_hash);
            silverfs_write(&file, buf, strlen(buf));
        }
        silverfs_close(&file);
    }
}

bool user_create(const char *username, const char *password) {
    if (num_users >= MAX_USERS) return false;
    
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) return false;
    }
    
    int new_uid = num_users;
    users[new_uid].uid = new_uid;
    strncpy(users[new_uid].username, username, MAX_USERNAME - 1);
    users[new_uid].username[MAX_USERNAME - 1] = '\0';
    
    /* Store password directly for demo */
    strncpy(users[new_uid].password_hash, password, MAX_PASSWORD_HASH - 1);
    users[new_uid].password_hash[MAX_PASSWORD_HASH - 1] = '\0';
    
    num_users++;
    save_users();
    
    serial_printf("[USER] Created user '%s' (uid=%d)\n", username, new_uid);
    return true;
}

bool user_login(const char *username, const char *password) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            if (strcmp(users[i].password_hash, password) == 0) {
                current_uid = users[i].uid;
                serial_printf("[USER] User '%s' logged in.\n", username);
                return true;
            }
            break;
        }
    }
    serial_printf("[USER] Failed login for user '%s'.\n", username);
    return false;
}

int user_get_current_uid(void) {
    return current_uid;
}

const char *user_get_current_username(void) {
    for (int i = 0; i < num_users; i++) {
        if (users[i].uid == current_uid) {
            return users[i].username;
        }
    }
    return "guest";
}
