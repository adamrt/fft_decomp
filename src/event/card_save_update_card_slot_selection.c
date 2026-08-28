#include "fft/battle.h"
#include "fft/card.h"
#include "psx/types.h"

enum {
    CARD_LAST_WRITTEN_SAVE_SLOT_NONE = 0xff,
};

void card_save_update_card_slot_selection(void) {
    if (g_card_save_slot_selection_thread_state == 0) {
        g_card_save_slot_selection_thread_state = 1;
        g_card_save_list_cursor_index = 0;
        g_card_save_card_slot_selection_descriptor[1].selected_index = g_card_save_selected_slot;
        g_card_save_slot_index = -1;
    }

    g_card_save_slot_selection_thread_state = card_thread_start_managed(6, g_card_save_card_slot_selection_descriptor);
    if (g_card_save_slot_selection_thread_state == 0) {
        if (g_card_save_slot_index == -1) {
            if (g_card_save_completed != 0) {
                g_card_save_menu_state = 10;
            } else {
                g_card_save_menu_state = 0;
            }
        } else {
            g_battle_text_substitution_values[0] = g_card_save_slot_index + 1;
            if (g_card_save_selected_slot != g_card_save_slot_index) {
                g_card_save_last_written_slot = CARD_LAST_WRITTEN_SAVE_SLOT_NONE;
            }
            g_card_save_selected_slot = g_card_save_slot_index;
            card_io_set_selected_slot(g_card_save_slot_index);
            g_card_save_menu_state = CARD_SAVE_MENU_SCANNING;
        }
    }
}
