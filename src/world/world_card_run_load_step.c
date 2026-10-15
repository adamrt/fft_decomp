#include "fft/script.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Advance the selected save slot's read operation and error prompt.
 *
 * The read starts after the loading message has been displayed. A complete
 * read must also pass world_card_load_globals_from_save_image before the loaded-slot transition.
 */
void world_card_run_load_step(s32 slot) {
    u32 state = g_world_card_load_step_state;
    s32 input;
    s32 bytes_read;
    if (state == 0) {
        g_world_card_load_step_state = 1;
        g_world_card_load_failed = 0;
        D_801CC7D0 = 0;
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_world_card_loading_message);
        memset(g_world_load_work_buffer, 0xFF, 0x1E80);
        return;
    }
    if (state < 2) {
        g_world_card_load_step_state = state + 1;
        return;
    }
    if (g_world_card_load_failed != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_world_card_load_error_message);
        input = g_menu_input_primary_repeat;
        if ((input & PSX_PAD_CIRCLE) || (input & PSX_PAD_CROSS)) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_world_card_load_step_state = 0;
            g_world_card_menu_step = WORLD_CARD_MENU_STEP_SCANNING;
        }
    } else {
        bytes_read
            = world_card_read_file_chunked(g_world_card_save_file_names[slot], (u8*)g_world_load_work_buffer, 0x1E00);
        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
        if (bytes_read == 0x1E00) {
            if (world_card_load_globals_from_save_image(0) != 0) {
                g_world_card_menu_result = 1;
                g_world_card_load_step_state = 0;
                g_world_card_menu_step = WORLD_CARD_MENU_STEP_LEAVE;
                script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
                g_world_menu_sound_effect_id = MAIN_SFX_COMPLETE;
                g_main_card_save_last_used_slot = slot;
                return;
            }
        } else {
            g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
        }
        g_world_card_load_failed = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_ERROR_MESSAGE;
    }
}
