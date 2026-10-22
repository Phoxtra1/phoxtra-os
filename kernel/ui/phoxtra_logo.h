#ifndef PHOXTRA_LOGO_H
#define PHOXTRA_LOGO_H

#include <stdint.h>
#include "../drivers/framebuffer.h"

/* Compact 40x40 raster derived from the transparent Phoxtra logo supplied
 * for this project. Palette-indexed RLE keeps the branding asset lightweight.
 * Index zero is transparent; indices 1..16 refer to the RGB palette below.
 */
static const uint32_t phoxtra_logo_palette[17] = {
    0x00000000,
    0x00EFC927, 0x006DBE49, 0x00FE9908, 0x0095A136,
    0x001BCCEE, 0x000CB7F8, 0x0012A3C3, 0x00098BC7,
    0x00118D43, 0x00E47822, 0x00E3511A, 0x000A7976,
    0x000B5494, 0x00006ED9, 0x00000087, 0x00000000
};

static const char phoxtra_logo_rle_base64[] =
"EAADCQUCHQAGCQgCFwAJCQgCAQcCDhMACgkHAgEEAQcDDhEACgkIAgEEAQcEDg4AAgEKCQgCAQQBBwIGBA4LAAEBAgMBAQEECAkIAgEEAQcDBgQOCgAEAwIBAQIGCQgCAQQBBwQGAw4JAAYDAgEBBAUJCAIBBAEHBQYDDgcACAMDAQEEAwkIAgEEAQcFBgQOBQABCgkDBAEBBAkCAQQBBwYGAw4FAAEKCQMFAQkCAQQBBwYGBA4EAAEDAQoJAwIBBQAHAgEHBwYDDgMABAoHAwIBBwAGAgEHBwYEDgIAAgsCCgYDAQIBBQoABAIBBwcGAw4BDQIAAwsCCgIDAQoBAgMFDAACAgEFCAYBCAILAQAGCwEKAQIBBQIGAgUOAAIFBQYBBwEECQsBBAEHAwYDBQ4AAgUEBgECAQoICwEEAQcFBgMFDgACBQIGAQIBCgEDAQoHCwEICAYCBQ4AAgUBBwEEAgMCCgULAQgDDgcGAgUOAAEFAQQFAwEKBQsEDgcGAgUNAAEBBwMBCgULBA4HBgIFCwACAQgDAQoFCwUOBwYBBQoAAgEJAwIKBAsFDggGCAADAQoDAgoDCwIABA4IBgIAAQkDAAYBCQMCCgMLAgAFDgcGAgADCQEEBgEJAwIKAwsCAAYOBgYCAAQJAQIFAQkDAgoCCwQABg4FBgIABgkBAgMBCgMBCgILBAAGDgUGAgAICQECAQEKAwMKBQAHDgMGAgAKCQECAQEIAwIKBwAHDgIGAgAMCQEEBwMBCggABw4BBgEOAgAOCQEEBQMBCgkACA4CABAJAQQCAwEKCwAHDgIAEgkBBA0ABg4CABIJEAAEDgIAEAkTAAMOAgAPCRkADQkbAAoJDwA=";

static uint8_t phoxtra_logo_pixels[40 * 40];
static uint8_t phoxtra_logo_decoded = 0;

static int phoxtra_logo_b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static void phoxtra_logo_prepare(void) {
    if (phoxtra_logo_decoded) return;
    uint8_t rle[700];
    uint32_t bytes = 0;
    uint32_t value = 0;
    int bits = 0;
    for (uint32_t i = 0; phoxtra_logo_rle_base64[i]; ++i) {
        int v = phoxtra_logo_b64_value(phoxtra_logo_rle_base64[i]);
        if (v < 0) continue;
        value = (value << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (bytes < sizeof(rle)) rle[bytes++] = (uint8_t)((value >> bits) & 0xFFu);
        }
    }
    uint32_t out = 0;
    for (uint32_t i = 0; i + 1 < bytes && out < 1600; i += 2) {
        uint8_t run = rle[i];
        uint8_t index = rle[i + 1];
        while (run-- && out < 1600) phoxtra_logo_pixels[out++] = index;
    }
    while (out < 1600) phoxtra_logo_pixels[out++] = 0;
    phoxtra_logo_decoded = 1;
}

static void phoxtra_logo_draw(uint32_t x, uint32_t y, uint32_t size) {
    if (!size) return;
    phoxtra_logo_prepare();
    for (uint32_t dy = 0; dy < size; ++dy) {
        uint32_t sy = dy * 40u / size;
        for (uint32_t dx = 0; dx < size; ++dx) {
            uint8_t index = phoxtra_logo_pixels[sy * 40u + dx * 40u / size];
            if (index != 0) fb_put_pixel(x + dx, y + dy, phoxtra_logo_palette[index]);
        }
    }
}

#endif
