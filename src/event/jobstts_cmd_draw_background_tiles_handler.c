#include "fft/jobstts.h"

/* The target passes tpage and clut at full ABI width; the callee truncates
 * them, so each call goes through a widened function-pointer type. */
#define jobstts_gfx_enqueue_textured_quad_wide                                                                         \
    ((void (*)(urect16_t*, s32, s32, u8*, s32, s32, s32, s32))jobstts_gfx_enqueue_textured_quad)

/* Fill a background rectangle with 16-pixel textured tiles.
 *
 * Command bytes 5 and 6 encode the full-tile counts in their high nibbles and
 * the remaining width and height in their low nibbles. The target passes tpage
 * and clut at full ABI width; the callee truncates them.
 */
u8* jobstts_cmd_draw_background_tiles_handler(u8* stream) {
    urect16_t rect;
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

    packed_height = stream[6];
    packed_width = stream[5];
    otag_index = g_jobstts_gfx_otag_index;
    clut_id = g_jobstts_gfx_background_clut_id;
    full_columns = packed_width >> 4;
    partial_width = packed_width & 0xF;
    full_rows = packed_height >> 4;
    partial_height = packed_height & 0xF;
    origin_x = stream[3];
    origin_y = stream[4];

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
            jobstts_gfx_enqueue_textured_quad_wide(&rect, 8, 8, 0, g_jobstts_gfx_semitransparency,
                g_jobstts_gfx_background_texture_page, row_clut_id, otag_index);
            column = 0;
            if (full_columns != 0) {
                current_row_y = tile_y;
                do {
                    rect.x = full_tile_x + (column * 16);
                    rect.y = current_row_y;
                    rect.w = 16;
                    rect.h = 16;
                    jobstts_gfx_enqueue_textured_quad_wide(&rect, 8, 8, 0, g_jobstts_gfx_semitransparency,
                        g_jobstts_gfx_background_texture_page, row_clut_id, otag_index);
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
        jobstts_gfx_enqueue_textured_quad_wide(
            &rect, 8, 8, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut_id, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = origin_x + partial_width + (column * 16);
            rect.y = origin_y + (row * 16);
            rect.w = 16;
            rect.h = partial_height;
            jobstts_gfx_enqueue_textured_quad_wide(&rect, 8, 8, 0, g_jobstts_gfx_semitransparency,
                g_jobstts_gfx_background_texture_page, clut_id, otag_index);
        }
    }

    /* Keeps the two unknown padding words in the target's 0xa8 frame. */
    __asm__ volatile("" : : "m"(stack_padding_0), "m"(stack_padding_1));
    return stream + stream[1];
}
