#include "fft/card.h"
#include "psx/types.h"

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
