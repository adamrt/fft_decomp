#include "fft/event_card.h"

/* Save-menu state CARD_SAVE_MENU_INITIAL_PROMPT: after 40 frames, runs the
 * "save the game?" menu. Choosing row 0 goes on to card selection; anything
 * else leaves the screen. */
void card_save_update_initial_prompt(void) {
    if (g_card_save_initial_prompt_thread_state == 0) {
        if (g_card_save_initial_prompt_timer < 40) {
            g_card_save_initial_prompt_timer++;
        } else {
            g_card_save_initial_prompt_thread_state = 1;
            g_card_save_list_cursor_index = 0;
            g_card_save_initial_prompt_descriptor[1].selected_index = 0;
            g_card_save_slot_index = -1;
        }
    } else {
        g_card_save_initial_prompt_thread_state
            = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_card_save_initial_prompt_descriptor);
        if (g_card_save_initial_prompt_thread_state == 0) {
            if (g_card_save_slot_index != 0) {
                g_card_save_menu_state = CARD_SAVE_MENU_LEAVE;
            } else {
                g_card_save_menu_state = CARD_SAVE_MENU_SELECT_CARD;
            }
        }
    }
}
