#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Confirm a name, using the original name when the edit buffer is empty.
 *
 * Names use 0xfe termination and contain at most 16 encoded bytes. Canceling
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
            while (value != TEXT_END_WAIT_FOR_CONFIRM) {
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
        g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_world_name_entry_text;
        g_text_substitution_values[0] = 0xc800;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xc01a, 0);
        g_world_name_confirm_initialized = 1;
        g_world_name_confirm_finished = 0;
        g_world_name_confirm_result = -1;
        g_world_menu_prompt_active = 1;
    }
    if (g_world_name_confirm_finished != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_name_confirm_initialized = 0;
            g_world_name_entry_state = 0xff;
            g_world_menu_prompt_active = 0;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_name_confirm_menu) == 0) {
        if (g_world_name_confirm_result == 0) {
            g_world_name_confirm_finished = 1;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, -1, 0);
            return;
        }
        g_world_name_entry_state = g_world_name_entry_saved_state;
        if (g_world_name_used_original_name != 0) {
            g_world_name_entry_text[0] = TEXT_END_WAIT_FOR_CONFIRM;
        }
        g_world_name_confirm_initialized = 0;
    }
}
