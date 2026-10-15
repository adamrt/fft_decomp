#include "fft/world.h"
#include "psx/types.h"

/* Builds a text command buffer from text: every glyph is preceded by an
 * 0xE7 x-position control code, and the running x advances by the glyph's
 * width plus a kerning pad taken from the width class; 0xFA is a 6-pixel
 * space and 0xFE terminates. The buffer is then handed to the image-row
 * renderer
 * with the caller's name, rect and palette passed through unchanged. */
void world_text_kern_into_command_buffer(u8* text, s16* name, RECT* rect, s32 palette) {
    u8 buf[0x78];
    u8* out;
    s32 len;
    s32 x;
    s32 pre;
    s32 post;
    s32 glyph;
    u8 character;
    u8 second;
    u8 code;

    len = 0;
    x = 0;
    glyph = *text++;
    character = glyph;
    if (glyph != TEXT_END_WAIT_FOR_CONFIRM) {
        code = TEXT_SET_X_POSITION;
        out = buf;
        do {
            if (character == TEXT_SPACE) {
                x += 6;
            } else {
                if (character >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    glyph = character & 0xF;
                    second = *text++;
                    glyph = second + glyph * TEXT_GLYPHS_PER_PREFIX;
                }
                glyph = g_text_glyph_widths[glyph];
                if (glyph == 2) {
                    pre = 2;
                    post = 2;
                } else if (glyph == 4) {
                    pre = 1;
                    post = 1;
                } else if (glyph == 3) {
                    pre = 1;
                    post = 2;
                } else {
                    post = 0;
                    pre = 0;
                }
                x += pre;
                *out++ = code;
                len++;
                *out++ = x;
                len++;
                *out++ = character;
                len++;
                if (character >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    *out++ = second;
                    len++;
                }
                x += glyph;
                x += post;
            }
            *out++ = code;
            len++;
            *out++ = x;
            len++;
            glyph = *text++;
            character = glyph;
        } while (glyph != TEXT_END_WAIT_FOR_CONFIRM);
    }
    buf[len] = TEXT_END_WAIT_FOR_CONFIRM;
    world_text_render_id_list_to_image_rows(buf, name, rect, palette);
}
