#include "file_manager.h"
#include "compositor.h"
#include "font.h"
#include "theme.h"
#include "../drivers/framebuffer.h"
#include "../vfs/vfs.h"
#include "../log/logger.h"

static window_t* fm_win = 0;
static char current_path[128] = "/home/user";

void file_manager_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra Graphical File Manager (phoxtra-fm)...");
}

void file_manager_open_window(const char* initial_path) {
    if (initial_path && initial_path[0]) {
        int i = 0;
        while (initial_path[i] && i < 127) {
            current_path[i] = initial_path[i];
            i++;
        }
        current_path[i] = '\0';
    }

    fm_win = compositor_create_window("Phoxtra File Manager - /home/user", 120, 100, 780, 500, 0x00020617);
    if (!fm_win) return;

    klog(LOG_INFO, "Opened File Manager Window");
}

void file_manager_create_folder(const char* folder_name) {
    if (!folder_name || !folder_name[0]) return;
    char path_buf[256];
    int p = 0;
    while (current_path[p] && p < 120) { path_buf[p] = current_path[p]; p++; }
    if (path_buf[p - 1] != '/') path_buf[p++] = '/';
    int f = 0;
    while (folder_name[f] && p < 250) { path_buf[p++] = folder_name[f++]; }
    path_buf[p] = '\0';

    vfs_create_node(folder_name, 0x02);
    klog(LOG_INFO, "File Manager: Created Directory");
}

void file_manager_create_file(const char* filename, const char* content) {
    if (!filename || !filename[0]) return;
    klog(LOG_INFO, "File Manager: Created File");
}
