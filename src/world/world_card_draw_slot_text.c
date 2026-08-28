#include "fft/world.h"
#include "psx/types.h"

/* Memory-card save-slot records (0x8C bytes each); 15 slots laid out in
 * three columns of five on the memory card menu. */

void world_card_draw_slot_text(s32 save_slot_index) {
    RECT destination_rect;

    if (save_slot_index < 6) {
        destination_rect.x = 0x240;
        destination_rect.y = (save_slot_index << 5) + 0x130;
    } else if (save_slot_index < 11) {
        destination_rect.x = 0x1C0;
        destination_rect.y = (save_slot_index << 5) - 0x80;
    } else {
        destination_rect.x = 0x180;
        destination_rect.y = (save_slot_index << 5) - 0x100;
    }
    destination_rect.w = 0x32;
    destination_rect.h = 0x20;
    /* Keeps the last rect store ahead of the slot-address multiply. */
    __asm__ volatile("");
    world_text_render_id_list_to_image_rows(g_world_card_save_slot_descriptions[save_slot_index],
        (s16*)g_world_card_slot_description_row_ids, &destination_rect, 0);
}
