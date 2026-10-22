#ifndef PHOXTRA_FILE_MANAGER_H
#define PHOXTRA_FILE_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void file_manager_init(void);
void file_manager_render(void);
void file_manager_open_window(const char* initial_path);
void file_manager_create_folder(const char* folder_name);
void file_manager_navigate(const char* path);
int file_manager_open_entry(uint32_t index);
void file_manager_create_file(const char* filename, const char* content);

#ifdef __cplusplus
}
#endif

#endif
