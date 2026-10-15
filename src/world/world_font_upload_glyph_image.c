#include "fft/battle_text.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Renders one encoded glyph (either a direct id or a 0xD?-prefixed
 * two-byte code) into a 0x78-byte 4bpp image and uploads it at (x, y). */
void world_font_upload_glyph_image(u16 code, s32 x, s32 y, s32 style) {
    u8 image[0x78];
    RECT rect;
    u32 glyph;

    memset(image, 0, sizeof(image));
    rect.x = 0;
    rect.y = 0;
    rect.w = 3;
    rect.h = 0x10;
    if (code > 0xcfff) {
        glyph = (code >> 8) & 0xf;
        glyph = glyph * TEXT_GLYPHS_PER_PREFIX + (code & 0xff);
    } else {
        if (code >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            return;
        }
        glyph = code;
    }
    menu_text_render_glyph_to_4bpp_image_variant(glyph, image, (const battle_text_image_position_t*)&rect, style);
    rect.x = x;
    rect.y = y;
    LoadImage(&rect, image);
    while (DrawSync(1) != 0) { }
}
