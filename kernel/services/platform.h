#ifndef PHOXTRA_PLATFORM_SERVICES_H
#define PHOXTRA_PLATFORM_SERVICES_H

#include <stdint.h>
#include <stddef.h>
#include "../process/pcb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_NAME_CAP 48u
#define PLATFORM_ID_CAP 32u
#define PLATFORM_DESCRIPTION_CAP 80u
#define PLATFORM_FS_NAME_CAP 64u

typedef enum {
    PLATFORM_OK = 0,
    PLATFORM_ERR_INVALID = -1,
    PLATFORM_ERR_NOT_FOUND = -2,
    PLATFORM_ERR_UNSUPPORTED = -3,
    PLATFORM_ERR_DENIED = -4,
    PLATFORM_ERR_IO = -5
} platform_result_t;

/* Shared input contract. Applications consume normalized events, not PS/2 ports. */
typedef enum {
    PLATFORM_INPUT_NONE = 0,
    PLATFORM_INPUT_KEY = 1,
    PLATFORM_INPUT_POINTER = 2
} platform_input_type_t;

#define PLATFORM_INPUT_LEFT_PRESSED 0x01u
#define PLATFORM_INPUT_RIGHT_PRESSED 0x02u
#define PLATFORM_INPUT_MIDDLE_PRESSED 0x04u
#define PLATFORM_INPUT_LEFT_CLICKED 0x08u
#define PLATFORM_MOD_CTRL 0x01u

typedef struct platform_input_event {
    uint8_t type;
    uint8_t key;
    uint8_t buttons;
    uint8_t modifiers;
    int32_t x;
    int32_t y;
} platform_input_event_t;

/* Returns 1 for an event, 0 when no event is queued, and a negative error code. */
int platform_input_poll(platform_input_event_t* event);
int platform_input_pointer_position(int32_t* x, int32_t* y);
int platform_input_has_keyboard(void);
int platform_input_has_pointer(void);

/* Display boundary. The framebuffer is currently the only display backend. */
typedef struct platform_display_info {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bits_per_pixel;
    uint8_t available;
} platform_display_info_t;

int platform_display_get_info(platform_display_info_t* info);
int platform_display_is_available(void);
void platform_display_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void platform_display_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void platform_display_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void platform_display_draw_gradient(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t start_color, uint32_t end_color);
void platform_display_clear(uint32_t color);
void platform_display_present(void);

/* Storage contract. Node handles are opaque outside the VFS service. */
typedef struct vfs_node platform_fs_node_t;
typedef enum {
    PLATFORM_FS_ENTRY_FILE = 1,
    PLATFORM_FS_ENTRY_DIRECTORY = 2,
    PLATFORM_FS_ENTRY_DEVICE = 3
} platform_fs_entry_type_t;

typedef struct platform_fs_entry {
    char name[PLATFORM_FS_NAME_CAP];
    uint32_t type;
    uint64_t size;
    platform_fs_node_t* handle;
} platform_fs_entry_t;

typedef struct platform_storage_info {
    uint32_t mode;
    uint8_t persistent;
    const char* description;
} platform_storage_info_t;

#define PLATFORM_STORAGE_VOLATILE_RAM 1u
platform_fs_node_t* platform_storage_lookup(const char* path);
int platform_storage_is_directory(const platform_fs_node_t* node);
int platform_storage_list(const platform_fs_node_t* directory, uint32_t index, platform_fs_entry_t* entry);
uint32_t platform_storage_count(const platform_fs_node_t* directory);
uint64_t platform_storage_read(const platform_fs_node_t* file, uint64_t offset, uint64_t size, uint8_t* buffer);
uint64_t platform_storage_write(platform_fs_node_t* file, uint64_t offset, uint64_t size, const uint8_t* buffer);
int platform_storage_create_directory(platform_fs_node_t* parent, const char* name);
int platform_storage_create_text_file(platform_fs_node_t* parent, const char* name, const char* initial_contents);
int platform_storage_get_info(platform_storage_info_t* info);

/* Application registry is metadata only: registered does not imply an OS process launcher exists. */
typedef enum {
    PLATFORM_APP_EMBEDDED_PANEL = 1,
    PLATFORM_APP_PLANNED_APPLICATION = 2
} platform_app_kind_t;

typedef enum {
    PLATFORM_APP_AVAILABLE = 1,
    PLATFORM_APP_PLANNED = 2
} platform_app_state_t;

typedef struct platform_app_info {
    const char* id;
    const char* name;
    const char* category;
    const char* description;
    uint32_t kind;
    uint32_t state;
} platform_app_info_t;

uint32_t platform_app_count(void);
int platform_app_get(uint32_t index, const platform_app_info_t** info);
int platform_app_find(const char* id, const platform_app_info_t** info);

/* Hardware inventory is based on enumerated PCI data, never fabricated product names. */
typedef struct platform_device_info {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_id;
    uint8_t subclass;
    const char* class_name;
} platform_device_info_t;

uint32_t platform_device_count(void);
int platform_device_get(uint32_t index, platform_device_info_t* info);

/* Security checks fail closed for missing caller/node context. */
int platform_security_check_vfs(const pcb_t* caller, const platform_fs_node_t* node, uint32_t mode);
int platform_security_has_capability(pcb_t* caller, uint64_t capability);

/* Product profile makes per-product defaults explicit without forking shared services. */
typedef enum {
    PLATFORM_PROFILE_LAPTOP = 1,
    PLATFORM_PROFILE_DESKTOP = 2,
    PLATFORM_PROFILE_EMBEDDED = 3
} platform_product_kind_t;

typedef struct platform_product_profile {
    uint32_t kind;
    const char* id;
    const char* display_name;
    uint8_t has_pointing_device_expected;
    uint8_t persistent_storage_expected;
    uint8_t touch_input_optional;
} platform_product_profile_t;

const platform_product_profile_t* platform_product_profile(void);

/* Call once after drivers, VFS, and security policy providers have initialized. */
int platform_services_init(void);
int platform_services_is_initialized(void);
uint32_t platform_services_status_flags(void);

#define PLATFORM_STATUS_DISPLAY 0x0001u
#define PLATFORM_STATUS_KEYBOARD 0x0002u
#define PLATFORM_STATUS_POINTER 0x0004u
#define PLATFORM_STATUS_VFS 0x0008u
#define PLATFORM_STATUS_PCI_INVENTORY 0x0010u
#define PLATFORM_STATUS_APP_REGISTRY 0x0020u
#define PLATFORM_STATUS_STORAGE_VOLATILE 0x0040u
#define PLATFORM_STATUS_POLICY_PROVIDER 0x0080u

#ifdef __cplusplus
}
#endif

#endif
