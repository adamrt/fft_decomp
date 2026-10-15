/* Text id list rendering to image rows, kerning into the command buffer, glyph image upload and entry advance by
 * character count. */

#include "fft/battle_text.h"
#include "fft/gfx.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Renders each text id of a -1 terminated list into its own 16 pixel line of
 * `destination`, uploading one line at a time through the scratch image at
 * g_world_text_row_image_buffer. */
void world_text_render_id_list_to_image_rows(u8* font, s16* text_ids, RECT* destination, s32 palette) {
    RECT scratch_rect;
    RECT destination_row;
    s16 row_text_ids[4];

    scratch_rect.x = 0;
    scratch_rect.y = 0;
    scratch_rect.w = destination->w;
    scratch_rect.h = 0x10;
    destination_row.x = destination->x;
    destination_row.y = destination->y;
    destination_row.w = destination->w;
    destination_row.h = 0x10;
    for (; *text_ids != -1; text_ids++) {
        row_text_ids[0] = *text_ids;
        row_text_ids[1] = -1;
        memset(g_world_text_row_image_buffer, 0, 0x800);
        menu_text_render_encoded_ids_to_image(
            g_world_text_row_image_buffer, &scratch_rect, 0xa, 0x64, font, row_text_ids, 0x64, 0xe7, palette);
        gfx_load_image_and_wait(&destination_row, g_world_text_row_image_buffer);
        destination_row.y += 0x10;
    }
}

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

/* Advance over count characters of a 0xFE-terminated text entry; bytes
 * TEXT_EXTENDED_GLYPH_PREFIX_FIRST..TEXT_EXTENDED_GLYPH_PREFIX_LAST carry a one-byte operand. */
u8* world_text_advance_entry_by_char_count(u8* text, s32 count) {
    s32 i;
    s32 offset;
    u8* cursor;

    offset = 0;
    for (i = 0; i < count; i++) {
        cursor = &text[offset];
        if (*cursor == TEXT_END_WAIT_FOR_CONFIRM) {
            break;
        }
        if (*cursor >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST && *cursor <= TEXT_EXTENDED_GLYPH_PREFIX_LAST) {
            offset += 2;
        } else {
            offset += 1;
        }
    }
    return text + offset;
}
