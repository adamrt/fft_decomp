#include "fft/event_card.h"

/*
 * Save-menu state CARD_SAVE_MENU_SELECT_CARD: runs the "slot 1 / slot 2" menu.
 *
 * Canceling returns to the initial prompt, or leaves the screen once a save
 * has been completed. Choosing a card slot selects it for the card I/O,
 * forgets the last used save slot when the card slot changed, and starts the
 * scan.
 */
void card_save_update_card_slot_selection(void) {
    if (g_card_save_slot_selection_thread_state == 0) {
        g_card_save_slot_selection_thread_state = 1;
        g_card_save_list_cursor_index = 0;
        g_card_save_card_slot_selection_descriptor[1].selected_index = g_main_card_save_selected_slot;
        g_card_save_slot_index = -1;
    }

    g_card_save_slot_selection_thread_state
        = card_thread_start_managed(THREAD_SCREEN_SLOT_MENU, g_card_save_card_slot_selection_descriptor);
    if (g_card_save_slot_selection_thread_state == 0) {
        if (g_card_save_slot_index == -1) {
            if (g_card_save_completed != 0) {
                g_card_save_menu_state = CARD_SAVE_MENU_LEAVE;
            } else {
                g_card_save_menu_state = CARD_SAVE_MENU_INITIAL_PROMPT;
            }
        } else {
            g_text_substitution_values[0] = g_card_save_slot_index + 1;
            if (g_main_card_save_selected_slot != g_card_save_slot_index) {
                g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
            }
            g_main_card_save_selected_slot = g_card_save_slot_index;
            card_io_set_selected_slot(g_card_save_slot_index);
            g_card_save_menu_state = CARD_SAVE_MENU_SCANNING;
        }
    }
}
