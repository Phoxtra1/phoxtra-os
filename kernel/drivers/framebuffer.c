#include "framebuffer.h"
#include "../memory/vmm.h"
#include "../log/logger.h"
#include <stddef.h>
#include <stdint.h>

#define MB2_TAG_END 0u
#define MB2_TAG_FRAMEBUFFER 8u
#define MB2_FRAMEBUFFER_RGB 1u
#define PHYSICAL_LIMIT_4G 0x100000000ULL

typedef struct __attribute__((packed)) { uint32_t total_size, reserved; } mb2_info_header_t;
typedef struct __attribute__((packed)) { uint32_t type, size; } mb2_tag_t;
typedef struct __attribute__((packed)) {
    uint32_t type, size;
    uint64_t address;
    uint32_t pitch, width, height;
    uint8_t bpp, framebuffer_type;
    uint16_t reserved;
    uint8_t red_position, red_mask_size;
    uint8_t green_position, green_mask_size;
    uint8_t blue_position, blue_mask_size;
} mb2_framebuffer_tag_t;

static framebuffer_t fb;
static uint32_t back_storage[FB_WIDTH * FB_HEIGHT];
static uint32_t boot_magic;
static uint32_t boot_info_phys;
static uint8_t red_position, red_mask_size, green_position, green_mask_size;
static uint8_t blue_position, blue_mask_size;

void framebuffer_set_boot_info(uint32_t magic, uint32_t info_phys) {
    boot_magic = magic;
    boot_info_phys = info_phys;
}

static const mb2_framebuffer_tag_t* find_framebuffer_tag(void) {
    if (boot_magic != MULTIBOOT2_BOOT_MAGIC || boot_info_phys == 0) {
        klog(LOG_ERROR, "Framebuffer: missing/invalid Multiboot2 boot information");
        return NULL;
    }
    const mb2_info_header_t* info = (const mb2_info_header_t*)(uintptr_t)boot_info_phys;
    if (info->total_size < sizeof(*info) || info->total_size > 16u * 1024u * 1024u) {
        klog(LOG_ERROR, "Framebuffer: invalid Multiboot2 information size");
        return NULL;
    }
    uint32_t offset = sizeof(*info);
    while (offset <= info->total_size - sizeof(mb2_tag_t)) {
        const mb2_tag_t* tag = (const mb2_tag_t*)((const uint8_t*)info + offset);
        if (tag->size < sizeof(*tag) || tag->size > info->total_size - offset) {
            klog(LOG_ERROR, "Framebuffer: malformed Multiboot2 tag");
            return NULL;
        }
        if (tag->type == MB2_TAG_END) return NULL;
        if (tag->type == MB2_TAG_FRAMEBUFFER &&
            tag->size >= offsetof(mb2_framebuffer_tag_t, red_position))
            return (const mb2_framebuffer_tag_t*)tag;
        uint32_t next = (tag->size + 7u) & ~7u;
        if (next < tag->size || next > info->total_size - offset) break;
        offset += next;
    }
    klog(LOG_ERROR, "Framebuffer: bootloader supplied no framebuffer tag");
    return NULL;
}

static uint32_t encode_channel(uint8_t value, uint8_t position, uint8_t bits) {
    if (!bits || bits > 8 || position >= 32 || position + bits > 32) return 0;
    uint32_t max_value = (1u << bits) - 1u;
    return ((((uint32_t)value * max_value + 127u) / 255u) << position);
}
static uint32_t encode_pixel(uint32_t color) {
    return encode_channel((color >> 16) & 255u, red_position, red_mask_size) |
           encode_channel((color >> 8) & 255u, green_position, green_mask_size) |
           encode_channel(color & 255u, blue_position, blue_mask_size);
}

void framebuffer_init(void) {
    fb.address = NULL; fb.back_buffer = back_storage;
    fb.width = fb.height = fb.pitch = fb.bpp = fb.size = 0;
    fb.available = 0;
    klog(LOG_INFO, "Initializing Multiboot2 framebuffer driver...");
    const mb2_framebuffer_tag_t* tag = find_framebuffer_tag();
    if (!tag) return;
    if (tag->framebuffer_type != MB2_FRAMEBUFFER_RGB || tag->bpp != 32 ||
        tag->size < sizeof(mb2_framebuffer_tag_t) || !tag->width || !tag->height ||
        tag->width > UINT32_MAX / 4u || tag->pitch < tag->width * 4u || !tag->address) {
        klog(LOG_ERROR, "Framebuffer: unsupported mode; expected 32-bit RGB pixels");
        return;
    }
    uint64_t bytes = (uint64_t)tag->pitch * tag->height;
    if (!bytes || tag->address >= PHYSICAL_LIMIT_4G ||
        bytes > PHYSICAL_LIMIT_4G - tag->address || bytes > UINT32_MAX) {
        klog(LOG_ERROR, "Framebuffer: address/size is outside the mapped 4 GiB range");
        return;
    }
    if (!tag->red_mask_size || !tag->green_mask_size || !tag->blue_mask_size ||
        tag->red_mask_size > 8 || tag->green_mask_size > 8 || tag->blue_mask_size > 8 ||
        tag->red_position + tag->red_mask_size > 32 ||
        tag->green_position + tag->green_mask_size > 32 ||
        tag->blue_position + tag->blue_mask_size > 32) {
        klog(LOG_ERROR, "Framebuffer: invalid RGB channel masks");
        return;
    }
    if (vmm_ensure_identity_mapped(tag->address, bytes) != 0) {
        klog(LOG_ERROR, "Framebuffer: physical framebuffer is not accessible");
        return;
    }
    if (vmm_mark_uncached(tag->address, bytes) != 0) {
        klog(LOG_ERROR, "Framebuffer: could not configure uncached MMIO mapping");
        return;
    }
    fb.address = (uint32_t*)(uintptr_t)tag->address;
    fb.width = tag->width < FB_WIDTH ? tag->width : FB_WIDTH;
    fb.height = tag->height < FB_HEIGHT ? tag->height : FB_HEIGHT;
    fb.pitch = tag->pitch;
    fb.bpp = tag->bpp;
    fb.size = (uint32_t)bytes;
    fb.available = 1;
    red_position = tag->red_position; red_mask_size = tag->red_mask_size;
    green_position = tag->green_position; green_mask_size = tag->green_mask_size;
    blue_position = tag->blue_position; blue_mask_size = tag->blue_mask_size;
    fb_clear(0x000F172A);
    fb_swap_buffers();
    klog(LOG_INFO, "Hardware framebuffer connected; double-buffered output enabled");
}

framebuffer_t* framebuffer_get(void) { return &fb; }
int framebuffer_is_available(void) { return fb.available != 0; }

uint32_t fb_get_back_pixel(uint32_t x, uint32_t y) {
    if (!fb.available || x >= fb.width || y >= fb.height) return 0;
    return back_storage[(size_t)y * FB_WIDTH + x];
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb.available || x >= fb.width || y >= fb.height) return;
    back_storage[(size_t)y * FB_WIDTH + x] = color;
}
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!fb.available || !width || !height) return;
    uint64_t r64 = (uint64_t)x + width, b64 = (uint64_t)y + height;
    uint32_t right = r64 > fb.width ? fb.width : (uint32_t)r64;
    uint32_t bottom = b64 > fb.height ? fb.height : (uint32_t)b64;
    if (x >= right || y >= bottom) return;
    for (uint32_t i=x; i<right; i++) {
        fb_put_pixel(i,y,color);
        if (bottom > y+1) fb_put_pixel(i,bottom-1,color);
    }
    for (uint32_t j=y; j<bottom; j++) {
        fb_put_pixel(x,j,color);
        if (right > x+1) fb_put_pixel(right-1,j,color);
    }
}
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!fb.available || !width || !height) return;
    uint64_t r64 = (uint64_t)x + width, b64 = (uint64_t)y + height;
    uint32_t right = r64 > fb.width ? fb.width : (uint32_t)r64;
    uint32_t bottom = b64 > fb.height ? fb.height : (uint32_t)b64;
    if (x >= right || y >= bottom) return;
    for (uint32_t j=y; j<bottom; j++)
        for (uint32_t i=x; i<right; i++)
            back_storage[(size_t)j * FB_WIDTH + i] = color;
}
void fb_draw_gradient(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                      uint32_t color1, uint32_t color2) {
    if (!fb.available || !width || !height) return;
    uint64_t r64 = (uint64_t)x + width, b64 = (uint64_t)y + height;
    uint32_t right = r64 > fb.width ? fb.width : (uint32_t)r64;
    uint32_t bottom = b64 > fb.height ? fb.height : (uint32_t)b64;
    if (x >= right || y >= bottom) return;
    int r1=(color1>>16)&255, g1=(color1>>8)&255, b1=color1&255;
    int r2=(color2>>16)&255, g2=(color2>>8)&255, b2=color2&255;
    uint32_t span = height > 1 ? height-1 : 1;
    for (uint32_t j=y; j<bottom; j++) {
        uint32_t step=j-y;
        uint32_t r=(uint32_t)(r1+((r2-r1)*(int)step)/(int)span);
        uint32_t g=(uint32_t)(g1+((g2-g1)*(int)step)/(int)span);
        uint32_t b=(uint32_t)(b1+((b2-b1)*(int)step)/(int)span);
        uint32_t c=(r<<16)|(g<<8)|b;
        for (uint32_t i=x; i<right; i++) back_storage[(size_t)j*FB_WIDTH+i]=c;
    }
}
void fb_clear(uint32_t color) {
    for (size_t i=0; i<(size_t)FB_WIDTH*FB_HEIGHT; i++) back_storage[i]=color;
}
void fb_swap_buffers(void) {
    if (!fb.available || !fb.address) return;
    for (uint32_t y=0; y<fb.height; y++) {
        uint32_t* dst=(uint32_t*)((uint8_t*)fb.address+(size_t)y*fb.pitch);
        const uint32_t* src=&back_storage[(size_t)y*FB_WIDTH];
        for (uint32_t x=0; x<fb.width; x++) dst[x]=encode_pixel(src[x]);
    }
}
