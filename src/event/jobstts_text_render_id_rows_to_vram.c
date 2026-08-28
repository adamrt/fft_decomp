#include "fft/jobstts.h"
#include "psx/types.h"

/* Render each text id of the -1 terminated `list` as one 16-pixel row in VRAM at `rect`. */
void jobstts_text_render_id_rows_to_vram(s32 text_table, s16* list, RECT* rect, s32 style) {
    u32 buffer[0x200];
    RECT clear_rect;
    RECT dest_rect;
    s16 ids[2];

    clear_rect.x = 0;
    clear_rect.y = 0;
    clear_rect.w = rect->w;
    clear_rect.h = 0x10;
    dest_rect.x = rect->x;
    dest_rect.y = rect->y;
    dest_rect.w = rect->w;
    dest_rect.h = 0x10;
    while (*list != -1) {
        ids[0] = *list;
        ids[1] = -1;
        memset(buffer, 0, 0x800);
        jobstts_text_render_encoded_ids_to_image((u8*)buffer, (const jobstts_text_image_bounds_t*)&clear_rect, 0xA,
            0x64, (const void*)text_table, (const u16*)ids, 0x64, 0xE7, style);
        jobstts_gfx_load_image_and_wait(&dest_rect, buffer);
        list += 1;
        dest_rect.y += 0x10;
    }
}
