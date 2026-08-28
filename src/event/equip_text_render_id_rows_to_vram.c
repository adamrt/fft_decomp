#include "fft/equip.h"
#include "fft/text.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* equip_text_image_bounds_t is {u16 x, u16 y, u16 row_stride, s16 line_height},
 * private to equip_text_render_encoded_ids_to_image.c; the RECT view here is
 * the same 8 bytes. Forward-declared so no header is needed. */
typedef struct equip_text_image_bounds equip_text_image_bounds_t;

/* Render each text id of the -1 terminated `list` as one 16-pixel row in VRAM at `rect`. */
void equip_text_render_id_rows_to_vram(void* text_table, s16* list, RECT* rect, s32 style) {
    u32 buf[0x200];
    RECT clear_rect;
    RECT dest_rect;
    s16 text[4];

    clear_rect.x = 0;
    clear_rect.y = 0;
    clear_rect.w = rect->w;
    clear_rect.h = 0x10;

    dest_rect.x = rect->x;
    dest_rect.y = rect->y;
    dest_rect.w = rect->w;
    dest_rect.h = 0x10;

    while (*list != -1) {
        text[0] = *list;
        text[1] = -1;
        memset(buf, 0, 0x800);
        equip_text_render_encoded_ids_to_image(buf, (const equip_text_image_bounds_t*)&clear_rect, 0xa, 0x64,
            text_table, (const u16*)text, 0x64, TEXT_FORMAT_EXPLICIT_ID, style);
        equip_gfx_load_image_and_wait(&dest_rect, buf);
        dest_rect.y += 0x10;
        list++;
    }
}
