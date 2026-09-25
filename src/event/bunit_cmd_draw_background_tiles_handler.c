#include "fft/event_bunit.h"
#include "psx/types.h"

/* The target passes tpage and clut at full ABI width; the callee truncates
 * them, so each call goes through a widened function-pointer type. */
#define bunit_gfx_enqueue_textured_quad_wide                                                                           \
    ((void (*)(RECT*, s32, s32, u8*, s32, s32, s32, s32))bunit_gfx_enqueue_textured_quad)

/* Fill a background rectangle with 16-pixel textured tiles.
 *
 * Command bytes 5 and 6 encode the full-tile counts in their high nibbles and
 * the remaining width and height in their low nibbles.
 *
 * This is the BUNIT.OUT twin of jobstts_cmd_draw_background_tiles_handler and
 * shares its shape, re-pointed at BUNIT's globals:
 *   - stack_padding_0/1 reserve the two extra local words the target's 0xa8
 *     frame has over the natural 0x98 one; without them every save/restore
 *     offset is short by 0x10.
 *   - row_clut_id is a second variable holding the same clut as clut_id: the
 *     target keeps the row copy and the tail copy in different registers, so
 *     one C variable cannot reproduce it.
 */

u8* bunit_cmd_draw_background_tiles_handler(u8* desc) {
    RECT rect;
    s32 full_columns;
    s32 current_row_y;
    s32 origin_x;
    s32 origin_y;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 otag_index;
    u16 clut_id;
    s32 row_clut_id;
    u32 packed_height;
    u32 packed_width;
    s32 row;
    s32 column;
    s32 tile_y;
    s32 full_tile_x;
    u32 stack_padding_0;
    u32 stack_padding_1;

    packed_height = desc[6];
    packed_width = desc[5];
    otag_index = g_bunit_gfx_otag_index;
    clut_id = g_bunit_text_metric_2;
    full_columns = packed_width >> 4;
    partial_width = packed_width & 0xF;
    full_rows = packed_height >> 4;
    partial_height = packed_height & 0xF;
    origin_x = desc[3];
    origin_y = desc[4];

    row = 0;
    if (full_rows != 0) {
        row_clut_id = clut_id;
        tile_y = origin_y;
        full_tile_x = origin_x + partial_width;
        do {
            rect.x = origin_x;
            rect.y = tile_y;
            rect.w = partial_width;
            rect.h = 16;
            bunit_gfx_enqueue_textured_quad_wide(&rect, 8, 8, 0, g_bunit_gfx_semitrans_enabled,
                g_bunit_text_digit_texture_page, row_clut_id, otag_index);
            column = 0;
            if (full_columns != 0) {
                current_row_y = tile_y;
                do {
                    rect.x = full_tile_x + (column * 16);
                    rect.y = current_row_y;
                    rect.w = 16;
                    rect.h = 16;
                    bunit_gfx_enqueue_textured_quad_wide(&rect, 8, 8, 0, g_bunit_gfx_semitrans_enabled,
                        g_bunit_text_digit_texture_page, row_clut_id, otag_index);
                    column++;
                } while (column < full_columns);
            }
            tile_y += 16;
            row++;
        } while (row < full_rows);
    }

    if (partial_height != 0) {
        rect.x = origin_x;
        rect.y = origin_y + (row * 16);
        rect.w = partial_width;
        rect.h = partial_height;
        bunit_gfx_enqueue_textured_quad_wide(
            &rect, 8, 8, 0, g_bunit_gfx_semitrans_enabled, g_bunit_text_digit_texture_page, clut_id, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = origin_x + partial_width + (column * 16);
            rect.y = origin_y + (row * 16);
            rect.w = 16;
            rect.h = partial_height;
            bunit_gfx_enqueue_textured_quad_wide(
                &rect, 8, 8, 0, g_bunit_gfx_semitrans_enabled, g_bunit_text_digit_texture_page, clut_id, otag_index);
        }
    }

    /* Keeps the two unknown padding words in the target's 0xa8 frame. */
    __asm__ volatile("" : : "m"(stack_padding_0), "m"(stack_padding_1));
    return desc + desc[1];
}
