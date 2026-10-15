#include "fft/battle_text.h"
#include "fft/event_card.h"
#include "psx/types.h"

/* Render save slot `slot`'s two-line description into its 0x32 x 0x20 strip
 * of VRAM: slots 0..5 at x 0x100, 6..10 at x 0x1c0 and 11..14 at x 0x180,
 * 0x20 rows apart. */
void card_save_render_slot_text(s32 slot) {
    RECT rect;

    if (slot < 6) {
        rect.x = 0x100;
        rect.y = slot * 0x20 + 0x30;
        rect.w = 0x32;
        rect.h = 0x20;
    } else if (slot < 11) {
        rect.x = 0x1c0;
        rect.y = (slot - 6) * 0x20 + 0x40;
        rect.w = 0x32;
        rect.h = 0x20;
    } else {
        rect.x = 0x180;
        rect.y = (slot - 11) * 0x20 + 0x60;
        rect.w = 0x32;
        rect.h = 0x20;
    }
    text_render_id_rows_to_vram(g_card_save_slot_descriptions[slot], g_card_save_slot_text_row_ids, &rect, 0);
}
