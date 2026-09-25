#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Thread body: loads overlay files 9 and 10, runs the companion overlay
 * with state 4 until it finishes, then restarts menu threads 9..14 and
 * captures the cursors of the icon-list threads 8 and 7.
 */
void world_menu_run_companion_overlay_9_10_thread(void) {
    s32 saved;
    s32 i;
    s32 j;

    world_menu_set_transition_active_flag();
    saved = g_world_unit_view_mode;
    world_unit_start_view_thread(0, 0xFF, 0xFF);
    g_world_menu_overlay_state = 1;
    world_thread_wait_for_10_to_13();
    world_bin_load_file(9);
    world_menu_retry_alloc_with_message(0x20000);
    world_bin_load_file(10);
    world_thread_suspend(8);
    world_thread_suspend(7);
    g_world_companion_overlay_state = 4;
    do {
        g_world_menu_new_button_input = 0;
        world_thread_yield();
    } while (g_world_companion_overlay_state != 0);
    g_world_menu_thread_menu_data = g_world_menu_system_entries;
    g_world_menu_action_slot_row_actions = -3;
    g_world_menu_sound_muted = 1;
    g_world_menu_new_button_input |= PSX_PAD_CIRCLE;
    if (g_world_companion_overlay_prims == 0) {
        world_thread_resume(8);
        world_thread_resume(7);
    }
    for (i = 9; i < 15; i++) {
        world_thread_set_parameters(i, 0, 0, 1);
    }
    world_thread_wait_frames(4);
    if (g_world_companion_overlay_prims != 0) {
        for (j = 0; j < 2; j++) {
            g_world_menu_pending_selection[j]
                = ((world_menu_icon_thread_param_t*)g_world_thread_contexts[8 - j].function_parameter_1)->cursor;
        }
    }
    world_menu_free_high_overlay();
    g_world_menu_overlay_state = 0;
    world_unit_start_view_thread(saved, 0xFF, 0xFF);
    g_world_menu_sound_muted = 0;
    g_world_menu_new_button_input = 0;
    world_menu_clear_transition_active_flag();
    world_thread_exit_current();
}
