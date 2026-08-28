#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/libc.h"
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
        /* The target passes a ninth, legacy word (the palette) that the callee never reads. */
        ((void (*)(u8*, RECT*, s32, s32, u8*, s16*, s32, s16, s32))world_text_render_ids_into_image)(
            g_world_text_row_image_buffer, &scratch_rect, 0xa, 0x64, font, row_text_ids, 0x64, 0xe7, palette);
        world_gfx_load_image_sync(&destination_row, (u32*)g_world_text_row_image_buffer);
        destination_row.y += 0x10;
    }
}
