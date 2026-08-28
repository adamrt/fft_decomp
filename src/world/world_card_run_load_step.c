#include "fft/card.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/libc.h"
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
        world_menu_start_or_poll_thread(6, g_world_card_loading_message);
        memset(g_world_load_work_buffer, 0xFF, 0x1E80);
        return;
    }
    if (state < 2) {
        g_world_card_load_step_state = state + 1;
        return;
    }
    if (g_world_card_load_failed != 0) {
        world_menu_start_or_poll_thread(6, g_world_card_load_error_message);
        input = g_world_input_primary_repeat;
        if ((input & PSX_PAD_CIRCLE) || (input & PSX_PAD_CROSS)) {
            world_menu_stop_thread_and_wait(6);
            g_world_card_load_step_state = 0;
            g_world_card_menu_step = 2;
        }
    } else {
        bytes_read
            = world_card_read_file_chunked(g_world_card_save_file_names[slot], (u8*)g_world_load_work_buffer, 0x1E00);
        world_menu_stop_thread_and_wait(6);
        if (bytes_read == 0x1E00) {
            if (world_card_load_globals_from_save_image(0) != 0) {
                g_world_card_menu_result = 1;
                g_world_card_load_step_state = 0;
                g_world_card_menu_step = 9;
                world_script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
                g_world_menu_sound_effect_id = 0x85;
                g_card_save_last_written_slot = slot;
                return;
            }
        } else {
            g_card_save_last_written_slot = 0xFF;
        }
        g_world_card_load_failed = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_CARD_ERROR;
    }
}
