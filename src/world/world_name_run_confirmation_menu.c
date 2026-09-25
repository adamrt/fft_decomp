#include "fft/world.h"

/*
 * Confirm a name, using the original name when the edit buffer is empty.
 *
 * Names use 0xfe termination and contain at most 16 encoded bytes. Cancelling
 * restores the empty-buffer marker if the original name was substituted.
 * Confirmation waits for the prompt thread to finish before reporting 0xff.
 */
void world_name_run_confirmation_menu(void) {
    s32 count;
    u8 value;
    if (g_world_name_confirm_initialized == 0) {
        g_world_name_confirm_menu.selected_index = 0;
        if (g_world_name_entry_text[0] == TEXT_END_WAIT_FOR_CONFIRM) {
            /* Sharing the flag/index temporary preserves the target's
             * initial register allocation without an explicit constraint. */
            count = 1;
            g_world_name_used_original_name = count;
            value = g_world_name_entry_original_name[0];
            count = 0;
            g_world_name_entry_text[0] = value;
            while (value != 0xfe) {
                if (count >= 16) {
                    g_world_name_entry_text[16] = TEXT_END_WAIT_FOR_CONFIRM;
                    break;
                }
                count++;
                value = g_world_name_entry_original_name[count];
                g_world_name_entry_text[count] = value;
            }
        } else {
            g_world_name_used_original_name = 0;
        }
        g_world_text_section_pointers[25] = g_world_name_entry_text;
        g_world_text_substitution_values[0] = 0xc800;
        world_thread_set_parameters(2, 0x21, 0xc01a, 0);
        g_world_name_confirm_initialized = 1;
        g_world_name_confirm_finished = 0;
        g_world_name_confirm_result = -1;
        g_world_menu_prompt_active = 1;
    }
    if (g_world_name_confirm_finished != 0) {
        if (world_thread_is_running(2) == 0) {
            g_world_name_confirm_initialized = 0;
            g_world_name_entry_state = 0xff;
            g_world_menu_prompt_active = 0;
        }
    } else if (world_menu_run_thread(15, &g_world_name_confirm_menu) == 0) {
        if (g_world_name_confirm_result == 0) {
            g_world_name_confirm_finished = 1;
            world_thread_set_parameters(2, 0x21, -1, 0);
            return;
        }
        g_world_name_entry_state = g_world_name_entry_saved_state;
        if (g_world_name_used_original_name != 0) {
            g_world_name_entry_text[0] = TEXT_END_WAIT_FOR_CONFIRM;
        }
        g_world_name_confirm_initialized = 0;
    }
}
