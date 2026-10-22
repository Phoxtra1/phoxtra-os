#ifndef PHOXTRA_LOGO_RLE_H
#define PHOXTRA_LOGO_RLE_H

#include <stdint.h>
#include "../drivers/framebuffer.h"

/* 40x40, transparent, palette-indexed raster sampled from the supplied
 * Phoxtra logo. The RLE stream stores run length + palette index as hex bytes.
 */
static const uint32_t phoxtra_logo_palette[16] = {
    0x00000000, 0x00E9C61B, 0x0071C03F, 0x00FEA306,
    0x0097AB32, 0x0032C0C4, 0x0006B5FE, 0x002F9D49,
    0x00029DEF, 0x00FE8407, 0x00F5690D, 0x00AF5F3C,
    0x00118180, 0x000187F4, 0x00058346, 0x00026FAE
};

static const char phoxtra_logo_rle_hex[] =
"0f00060704021b000107020e060707021600050e060706020105020d1300060e0507060201040107040d0f00080e0507060201040102050d0d0001010104060e0607060201040102060d0b00030301010104040e06070602010401020208050d0900050302010107020e06070602010401020308050d07000703020101040107010e050706020104010202060208050d06000209060303010104050706020104010503060208040d05000409060304010102030706020104010504060108050d04000409070304010102010707020104010504060208050d0200010a05090603020104000802010505060108050d0200030a04090403010108000602010505060208040d0100050a040901030101010501060a000402010506060108040d010b060a020901010105020601050c000202010506060108020d010c090a0105040601050e000105060601080105010b070a010b0105060601050e0006060105010b070a010b010c0108070601050e0001050306010501040109060a020b010d0308060601050e000206010501040309060a040d020807060e000105010201030409060a040d020807060d00010103030409060a050d010807060c00010104030509050a050d020806060a00020106030409050a060d010806060800030107030409050a060d02080506020001070300060106030409050a0100060d020804060200010e02070104060106030409040a010b0100070d020803060200020e03070104040106030409040a0200080d0208010601080200030e0107010e010701020104020106030409040a0300080d0108010601080200040e04070102010106030409030a0500080d02080200060e04070102010105030309030a0500090d01080200080e0207020e0102010103030409010a0700090d02000e0e01070101010304090900080d0200100e010703090b00070d0200120e01040d00060d0200120e0f00050d0200110e1200030d02000f0e19000d0e1b000a0e0f00";

static uint8_t phoxtra_logo_pixels[40 * 40];
static int phoxtra_logo_decoded = 0;

static uint8_t phoxtra_logo_hex_byte(const char* value) {
    uint8_t out = 0;
    for (int i = 0; i < 2; ++i) {
        char c = value[i];
        uint8_t digit = (c >= '0' && c <= '9') ? (uint8_t)(c - '0') :
                        (c >= 'a' && c <= 'f') ? (uint8_t)(c - 'a' + 10) : 0;
        out = (uint8_t)((out << 4) | digit);
    }
    return out;
}

static void phoxtra_logo_draw(uint32_t x, uint32_t y, uint32_t size) {
    if (size == 0) return;
    if (!phoxtra_logo_decoded) {
        uint32_t out = 0;
        const uint32_t hex_len = (uint32_t)(sizeof(phoxtra_logo_rle_hex) - 1);
        for (uint32_t i = 0; i + 3 < hex_len && out < 1600; i += 4) {
            const uint8_t run = phoxtra_logo_hex_byte(&phoxtra_logo_rle_hex[i]);
            const uint8_t index = phoxtra_logo_hex_byte(&phoxtra_logo_rle_hex[i + 2]);
            for (uint32_t n = 0; n < run && out < 1600; ++n)
                phoxtra_logo_pixels[out++] = index;
        }
        while (out < 1600) phoxtra_logo_pixels[out++] = 0;
        phoxtra_logo_decoded = 1;
    }

    for (uint32_t dy = 0; dy < size; ++dy) {
        const uint32_t sy = (dy * 40u) / size;
        for (uint32_t dx = 0; dx < size; ++dx) {
            const uint8_t index = phoxtra_logo_pixels[sy * 40u + (dx * 40u) / size];
            if (index != 0)
                fb_put_pixel(x + dx, y + dy, phoxtra_logo_palette[index]);
        }
    }
}
#endif
