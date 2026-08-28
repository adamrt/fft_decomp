#include "fft/battle_text.h"
#include "fft/card.h"
#include "fft/text.h"
#include "psx/types.h"

/* The trailing unused_style word is never read, exactly as in the EQUIP twin
 * equip_text_render_encoded_ids_to_image, whose ninth parameter carries the
 * same unused style value; card_text_render_id_rows_to_vram passes it. */
void card_text_render_encoded_ids_to_image(u8* image, const card_text_image_bounds_t* bounds, s32 glyph_spacing,
    s32 line_width, const void* glyph_data, const u16* text_ids, s32 max_glyphs, s32 terminator, s32 unused_style) {
    card_text_image_position_t position;
    s32 row;
    s32 remaining_glyphs = max_glyphs;
    s32 packed_terminator;
    s32 glyph_index;
    const u16* text_id_cursor;

    (void)glyph_spacing;
    position.row_stride = bounds->row_stride;

    row = 0;
    if (((s16)(*text_ids)) != (-1)) {
        packed_terminator = terminator << 16;
        text_id_cursor = text_ids;
        do {
            u16 text_id;
            s32 style;

            if (remaining_glyphs == 0) {
                break;
            }

            position.x = bounds->x;
            position.y = bounds->y + (row * bounds->line_height);
            text_id = *text_id_cursor;
            style = 0;

            if ((text_id & 0x2000) != 0) {
                const u16* current_text_id;

                glyph_index = 0;
                current_text_id = text_id_cursor;
                do {
                    glyph_index++;
                    position.x += card_text_render_glyph_to_4bpp_image(packed_terminator >> 16, image, &position,
                        (((s32)(((u32)(*current_text_id)) << 16)) >> 30) | style);
                } while (glyph_index < 8);
            } else {
                const u8* encoded = ((const u8* (*)(const void*, s32, s32))card_text_skip_encoded_segments)(
                    glyph_data, text_id & 0x0fff, 2);
                s32 code = *(encoded++);

                if (code < 0xfe) {
                    const u16* current_text_id = text_id_cursor;

                    do {
                        if ((code == TEXT_NEWLINE) || (glyph_index >= line_width)) {
                            position.x = bounds->x;
                            row++;
                            position.y = bounds->y + (row * bounds->line_height);
                        }

                        if (code < TEXT_FORMAT_FIRST) {
                            if (code >= 0xd0) {
                                code &= 0x0f;
                                code = (code * 0xd0) + (*(encoded++));
                            }
                            position.x += card_text_render_glyph_to_4bpp_image(
                                code, image, &position, (((s32)(((u32)(*current_text_id)) << 16)) >> 30) | style);
                        } else if (code == TEXT_SPACE) {
                            position.x += 4;
                        } else if (code == TEXT_SET_X_POSITION) {
                            position.x = bounds->x + (*(encoded++));
                        } else if (code == TEXT_SET_PALETTE) {
                            style = (*(encoded++)) >> 2;
                        }
                        code = *(encoded++);
                    } while (code < 0xfe);
                }
            }
            row++;

            text_id_cursor++;
            remaining_glyphs--;
        } while (((s16)(*text_id_cursor)) != (-1));
    }
}
