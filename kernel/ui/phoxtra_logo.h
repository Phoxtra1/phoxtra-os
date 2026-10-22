#ifndef PHOXTRA_LOGO_H
#define PHOXTRA_LOGO_H

#include <stdint.h>
#include "../drivers/framebuffer.h"

/* Transparent, palette-indexed 48x48 raster sampled from the supplied
 * Phoxtra brand mark. RLE keeps this source asset compact for the kernel.
 */
static const uint32_t phoxtra_logo_palette[16] = {
    0x00000000, 0x00EBCD28, 0x006DC159, 0x00FE9909,
    0x009BA041, 0x0011C2F8, 0x0019A3AD, 0x00098BCB,
    0x00128D43, 0x00DE761F, 0x00E65119, 0x00097A6E,
    0x00085494, 0x00006DD1, 0x00000072, 0x00000000
};
static const char phoxtra_logo_rle_base64[] =
"QQAGCAgCIAAJCAkCAQcaAAsICgICDQEHFwAMCAgCAQQBAgMNAQcUAAELDQgIAgEEAQIBBwQNAQcRAAEEDQgJAgEEAQIBBQIHBA0PAAIBAQMBBAsICQIBBAECAwUBBwQNDQAEAwIBCggJAgEEAQIDBQIHBA0LAAYDAgEBBAgICQIBBAECBAUCBwQNCQAIAwMBAQQGCAkCAQQBAgUFAgcEDQgACQMEAQECBAgJAgEEAQIGBQEHBA0HAAsDBQEBAgMICAIBBAECBwUBBwQNBgAMAwUBAQQBCAkCAQQBAgcFAgcDDQUAAQkNAwQBAgAJAgEEAQIIBQEHBA0EAAMJCgMDAQYABwIBBAECCAUBBwQNAwAECQoDAQEKAAcCCQUBBwQNAgABCQIKAgkHAwEEAQUMAAYCCQUBBwMNAQwCAAMKBAkDAwEEAwUOAAQCCQUBBwENAQYCCgEABQoCCQIDAQQBBgQFEAACAgkFAQYBCQoKAQkBBAEGBgURAAgFAQYBBAsKAQQBBggFEQAHBQEEAQkKCgEEAQcKBREABQUBAgEJAQMCCQgKAgcLBREAAwUBAgEJAwMCCQYKAQcDDQEHCgUSAAEGAQEGAwEJBgoEDQEHCgURAAEBCAMBCQYKBA0CBwkFDwABAQoDAQkGCgUNAQcJBQ0AAgELAwEJBgoFDQEHCQULAAMBDAMBCQYKAQAFDQEHCAUKAAQBDAMBCQUKAgAFDQIHBwUDAAIIAwAGAQwDAgkECgIABg0BBwcFAwADCAECBwENAwEJBAoDAAUNAgcGBQMABQgBBAYBDAMBCQMKBAAGDQIHBQUDAAcIAQQEAQwDAQkDCgUABg0CBwQFAwAJCAEEAgEMAwEJAgoGAAcNAgcDBQMACwgBBAEBCwMDCQcABw0CBwEFAQcDAA0IAQQKAwIJCAAJDQIHAwAPCAEECQMBCQkACQ0BBwMAEQgBBAYDAQkLAAkNAwATCAEEAwMBCQ0ACA0DABUIAQQBCQ8ABw0DABUIAQsRAAYNAwAVCBQABA0DABMIFwADDQMAEQgbAAENAwAPCCEADAhCAA==";
static uint8_t phoxtra_logo_pixels[48 * 48];
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
    uint8_t rle[800];
    uint32_t bytes = 0, value = 0;
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
    for (uint32_t i = 0; i + 1 < bytes && out < 2304; i += 2) {
        uint8_t run = rle[i], index = rle[i + 1];
        while (run-- && out < 2304) phoxtra_logo_pixels[out++] = index;
    }
    while (out < 2304) phoxtra_logo_pixels[out++] = 0;
    phoxtra_logo_decoded = 1;
}
static void phoxtra_logo_draw(uint32_t x, uint32_t y, uint32_t size) {
    if (!size) return;
    phoxtra_logo_prepare();
    for (uint32_t dy = 0; dy < size; ++dy) {
        uint32_t sy = dy * 48u / size;
        for (uint32_t dx = 0; dx < size; ++dx) {
            uint8_t index = phoxtra_logo_pixels[sy * 48u + dx * 48u / size];
            if (index) fb_put_pixel(x + dx, y + dy, phoxtra_logo_palette[index]);
        }
    }
}
#endif
