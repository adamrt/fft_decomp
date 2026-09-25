#include "fft/event_equip.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Render each text id of the -1 terminated `list` as one 16-pixel row in VRAM at `rect`. */
void equip_text_render_id_rows_to_vram(void* text_table, s16* list, RECT* rect, s32 style) {
    u32 buf[0x200];
    battle_menu_text_image_bounds_t clear_rect;
    RECT dest_rect;
    s16 text[4];

    clear_rect.x = 0;
    clear_rect.y = 0;
    clear_rect.row_stride = rect->w;
    clear_rect.line_height = 0x10;

    dest_rect.x = rect->x;
    dest_rect.y = rect->y;
    dest_rect.w = rect->w;
    dest_rect.h = 0x10;

    while (*list != -1) {
        text[0] = *list;
        text[1] = -1;
        memset(buf, 0, 0x800);
        equip_text_render_encoded_ids_to_image(
            buf, &clear_rect, 0xa, 0x64, text_table, (const u16*)text, 0x64, TEXT_FORMAT_EXPLICIT_ID, style);
        equip_gfx_load_image_and_wait(&dest_rect, buf);
        dest_rect.y += 0x10;
        list++;
    }
}
