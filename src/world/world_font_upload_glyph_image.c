#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Renders one encoded glyph (either a direct id or a 0xD?-prefixed
 * two-byte code) into a 0x78-byte 4bpp image and uploads it at (x, y). */
void world_font_upload_glyph_image(s32 code, s32 x, s32 y, s32 style) {
    u8 image[0x78];
    RECT rect;
    u32 glyph;
    u16 packed = code;

    memset(image, 0, sizeof(image));
    rect.w = 3;
    rect.h = 0x10;
    /* Keeps the scheduler from hoisting the u16 truncation of `code` above the rect.w/rect.h stores. */
    __asm__ volatile("");
    rect.x = 0;
    rect.y = 0;
    glyph = packed;
    if (glyph > 0xcfff) {
        glyph = (glyph >> 8) & 0xf;
        glyph = glyph * 0xd0 + (code & 0xff);
    } else if (glyph >= 0xd0) {
        return;
    }
    world_text_decode_font_glyph_to_4bpp(glyph, image, (world_text_glyph_target_t*)&rect, style);
    rect.x = x;
    rect.y = y;
    LoadImage(&rect, image);
    while (DrawSync(1) != 0) { }
}
