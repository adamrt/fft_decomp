#include "fft/event_card.h"
#include "psx/types.h"

/*
 * Keeps row `selection` inside the four visible rows of the save-slot list
 * and steps the list's scroll.
 *
 * At rest, a row above or below the window moves the top row by one and
 * starts a scroll. The animation offset then holds the signed pixels covered
 * so far, growing by 4 a call; after one row height (48) it folds into
 * g_card_save_list_scroll_base_y and returns to 0.
 */
void card_save_update_slot_scroll(s32 selection) {
    if (g_card_save_list_scroll_anim_offset == 0) {
        if (selection < g_card_save_list_top_row) {
            g_card_save_list_scroll_anim_offset = 4;
            g_card_save_list_top_row--;
        }
        if (selection >= g_card_save_list_top_row + 4) {
            g_card_save_list_scroll_anim_offset = -4;
            g_card_save_list_top_row++;
        }
    } else if (g_card_save_list_scroll_anim_offset < 0) {
        g_card_save_list_scroll_anim_offset -= 4;
        if (g_card_save_list_scroll_anim_offset < -47) {
            g_card_save_list_scroll_anim_offset = 0;
            g_card_save_list_scroll_base_y -= 48;
        }
    } else if (g_card_save_list_scroll_anim_offset > 0) {
        g_card_save_list_scroll_anim_offset += 4;
        if (g_card_save_list_scroll_anim_offset >= 48) {
            g_card_save_list_scroll_anim_offset = 0;
            g_card_save_list_scroll_base_y += 48;
        }
    }
}
