#ifndef PHOXTRA_FILE_MANAGER_H
#define PHOXTRA_FILE_MANAGER_H

#include <stdint.h>
#include "compositor.h"

#ifdef __cplusplus
extern "C" {
#endif

void file_manager_init(void);
void file_manager_open_window(const char* initial_path);
void file_manager_navigate(const char* path);
void file_manager_render(void);
window_t* file_manager_get_window(void);
int file_manager_hit_test(uint32_t x, uint32_t y);
int file_manager_entry_is_directory(uint32_t index);
void file_manager_select_entry(uint32_t index);
int file_manager_open_entry(uint32_t index);
void file_manager_go_parent(void);
void file_manager_scroll(int32_t pages);
void file_manager_create_folder(const char* folder_name);
void file_manager_create_file(const char* filename, const char* content);

#ifdef __cplusplus
}
#endif

#endif
