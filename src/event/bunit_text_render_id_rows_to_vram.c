#include "fft/bunit.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Render each id of a -1 terminated text-id list as one 16-pixel row and
 * upload the rows to VRAM from the top of `destination` down. */
void bunit_text_render_id_rows_to_vram(s32 text_table, u16* text_ids, RECT* destination, s32 flags) {
    u32 image[0x200];
    battle_menu_text_image_bounds_t bounds;
    RECT upload_rect;
    s16 row_text_ids[2];

    bounds.x = 0;
    bounds.y = 0;
    bounds.row_stride = destination->w;
    bounds.line_height = 0x10;
    upload_rect.x = destination->x;
    upload_rect.y = destination->y;
    upload_rect.w = destination->w;
    upload_rect.h = 0x10;

    while (*(s16*)text_ids != -1) {
        row_text_ids[0] = *text_ids;
        row_text_ids[1] = -1;
        memset(image, 0, 0x800);
        bunit_text_render_ids_into_image(image, &bounds, 0xA, 0x64, text_table, row_text_ids, 0x64, 0xE7, flags);
        bunit_gfx_load_image_and_wait(&upload_rect, image);
        upload_rect.y += 0x10;
        text_ids++;
    }
}
