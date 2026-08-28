#include "fft/card.h"
#include "psx/types.h"

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
        g_card_save_initial_prompt_thread_state = card_thread_start_managed(6, g_card_save_initial_prompt_descriptor);
        if (g_card_save_initial_prompt_thread_state == 0) {
            if (g_card_save_slot_index != 0) {
                g_card_save_menu_state = 10;
            } else {
                g_card_save_menu_state = 1;
            }
        }
    }
}
