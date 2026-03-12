#include "../include/desktop.h"
#include "../include/framebuffer.h"
#include "../include/font.h"
#include "../include/string.h"
#include "../include/silverfs.h"
#include "../include/serial.h"

#define FBROWSER_MAX_ENTRIES 64
#define FBROWSER_ITEM_HEIGHT 24

static int fb_window_id = -1;
static char current_path[256] = "/";

static silverfs_dirent_t current_entries[FBROWSER_MAX_ENTRIES];
static int num_entries = 0;
static int selected_index = -1;

/* Read directory contents */
static void fbrowser_refresh(void) {
    num_entries = silverfs_readdir(current_path, current_entries, FBROWSER_MAX_ENTRIES);
    if (num_entries < 0) {
        num_entries = 0;
        serial_printf("[FBROWSER] Error reading dir: %s\n", current_path);
    }
    selected_index = -1;
}

static void fbrowser_navigate(const char *new_path) {
    strncpy(current_path, new_path, sizeof(current_path) - 1);
    current_path[sizeof(current_path) - 1] = '\0';
    
    // Update window title
    window_t *win = window_get(fb_window_id);
    if (win) {
        snprintf(win->title, sizeof(win->title), "File Browser - %s", current_path);
    }
    
    fbrowser_refresh();
}

static void fbrowser_navigate_up(void) {
    if (strcmp(current_path, "/") == 0) return; // Already at root
    
    char new_path[256];
    strncpy(new_path, current_path, sizeof(new_path));
    
    // Find last slash
    int len = strlen(new_path);
    for (int i = len - 1; i >= 0; i--) {
        if (new_path[i] == '/') {
            if (i == 0) {
                new_path[1] = '\0'; // Root
            } else {
                new_path[i] = '\0';
            }
            break;
        }
    }
    fbrowser_navigate(new_path);
}

static void fbrowser_draw(int id) {
    window_t *win = window_get(id);
    if (!win) return;

    int cx = win->x + WINDOW_BORDER;
    int cy = win->y + TITLEBAR_HEIGHT + 2;
    int cw = win->width - WINDOW_BORDER * 2;
    int ch = win->height - TITLEBAR_HEIGHT - WINDOW_BORDER - 2;

    /* Background */
    fb_fill_rect(cx, cy, cw, ch, RGB(25, 25, 30));

    /* Header / Toolbar */
    int header_h = 30;
    fb_fill_rect(cx, cy, cw, header_h, RGB(45, 45, 55));
    fb_draw_line(cx, cy + header_h, cx + cw, cy + header_h, RGB(60, 60, 70));
    
    /* Back button (simple rect) */
    fb_fill_rect(cx + 5, cy + 5, 40, 20, RGB(70, 70, 80));
    font_draw_string(cx + 10, cy + 7, "Back", RGB(220, 220, 220));
    
    /* Current path text */
    font_draw_string(cx + 55, cy + 7, current_path, RGB(200, 200, 200));

    /* File List */
    int list_y = cy + header_h + 5;
    
    if (num_entries == 0) {
        font_draw_string(cx + 10, list_y, "(Empty folder)", RGB(150, 150, 150));
        return;
    }

    for (int i = 0; i < num_entries; i++) {
        int item_y = list_y + i * FBROWSER_ITEM_HEIGHT;
        if (item_y + FBROWSER_ITEM_HEIGHT > cy + ch) break; // clipping
        
        // Highlight logic
        if (i == selected_index) {
            fb_fill_rect(cx + 2, item_y, cw - 4, FBROWSER_ITEM_HEIGHT, RGB(60, 80, 120));
        }
        
        // Determine type to show icon
        silverfs_inode_t inode;
        char entry_path[512];
        if (strcmp(current_path, "/") == 0) {
            snprintf(entry_path, sizeof(entry_path), "/%s", current_entries[i].name);
        } else {
            snprintf(entry_path, sizeof(entry_path), "%s/%s", current_path, current_entries[i].name);
        }
        
        bool is_dir = false;
        if (silverfs_stat(entry_path, &inode) == 0) {
            is_dir = (inode.type == SILVERFS_TYPE_DIR);
        }
        
        // Draw icon
        if (is_dir) {
            fb_fill_rect(cx + 10, item_y + 4, 16, 12, RGB(200, 180, 80)); // Directory icon
            font_draw_string(cx + 35, item_y + 4, current_entries[i].name, RGB(240, 220, 100));
        } else {
            fb_fill_rect(cx + 12, item_y + 2, 12, 16, RGB(200, 200, 200)); // File icon
            font_draw_string(cx + 35, item_y + 4, current_entries[i].name, RGB(220, 220, 220));
            
            // Draw size
            char size_str[32];
            snprintf(size_str, sizeof(size_str), "%d B", inode.size);
            font_draw_string(cx + cw - 80, item_y + 4, size_str, RGB(150, 150, 150));
        }
    }
}

static void fbrowser_handle_click(int id, int mx, int my) {
    window_t *win = window_get(id);
    if (!win) return;
    
    int cx = win->x + WINDOW_BORDER;
    int cy = win->y + TITLEBAR_HEIGHT + 2;
    int header_h = 30;
    
    // Check if back button clicked
    if (mx >= cx + 5 && mx <= cx + 45 && my >= cy + 5 && my <= cy + 25) {
        fbrowser_navigate_up();
        return;
    }
    
    // Check if list item clicked
    int list_y = cy + header_h + 5;
    if (my >= list_y) {
        int index = (my - list_y) / FBROWSER_ITEM_HEIGHT;
        if (index >= 0 && index < num_entries) {
            if (selected_index == index) {
                // Double click simulation - open
                silverfs_inode_t inode;
                char entry_path[512];
                if (strcmp(current_path, "/") == 0) {
                    snprintf(entry_path, sizeof(entry_path), "/%s", current_entries[index].name);
                } else {
                    snprintf(entry_path, sizeof(entry_path), "%s/%s", current_path, current_entries[index].name);
                }
                
                if (silverfs_stat(entry_path, &inode) == 0) {
                    if (inode.type == SILVERFS_TYPE_DIR) {
                        fbrowser_navigate(entry_path);
                    } else {
                        // It's a file, maybe print to serial or open in terminal
                        serial_printf("[FBROWSER] Opening file not yet implemented: %s\n", entry_path);
                    }
                }
            } else {
                // Single click - select
                selected_index = index;
            }
        }
    }
}

static void fbrowser_handle_key(int id, char key) {
    (void)id;
    (void)key;
}

void file_browser_open(const char *path) {
    if (fb_window_id >= 0 && window_get(fb_window_id)) {
        fbrowser_navigate(path ? path : "/");
        return; // Already open, just bring to front / change path
    }

    fb_window_id = window_create("File Browser", 150, 120, 500, 350);
    if (fb_window_id >= 0) {
        window_set_draw_callback(fb_window_id, fbrowser_draw);
        window_set_key_callback(fb_window_id, fbrowser_handle_key);
        window_set_click_callback(fb_window_id, fbrowser_handle_click);
        fbrowser_navigate(path ? path : "/");
    }
}
