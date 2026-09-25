#include "fft/wldcore.h"

/* Clear system flag 0x10000 and act on the pending world-menu request flags.
 *
 * With none of 0x202c0000 pending, a selected proposition with a Text action
 * reopens its menu level and text (0x5800 + script value); otherwise pending
 * script requests are dispatched. Either way two audio commands follow. Flags
 * 0x40000 and 0x200000 are consumed after publishing g_main_saved_weather_variable as script
 * variable 0x23; each request ends in wldcore_menu_push_screen_transition_level. */
void wldcore_menu_restore_level_on_world_entry(void) {
    s32 flags;

    flags = g_main_system_flags & ~0x10000;
    g_main_system_flags = flags;
    if ((flags & 0x202C0000) == 0) {
        if (wldcore_script_process_conditional_set(
                g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_TEXT)
            != 0) {
            wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
            wldcore_menu_push_scrollable_text_window_level(g_wldcore_script_state.args[0] + 0x5800);
        } else {
            wldcore_menu_dispatch_pending_script_requests();
        }
        wldcore_sound_enqueue_audio_command(1, 0x11B);
        wldcore_sound_enqueue_audio_command(3, 4);
        return;
    }
    if (flags & 0x40000) {
        g_main_system_flags = flags ^ 0x40000;
        world_script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_main_saved_weather_variable);
        wldcore_menu_push_screen_transition_level(0x1D, 0, 0);
    } else if (flags & 0x200000) {
        g_main_system_flags = flags ^ 0x200000;
        world_script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_main_saved_weather_variable);
        g_wldcore_audio_queue.music.tracks[1] = 0x1B;
        g_wldcore_audio_queue.music.tracks[2] = 0x1A;
        g_wldcore_audio_queue.current_slot = 2;
        wldcore_menu_push_screen_transition_level(0x2D, 1, 0);
    } else if (flags & 0x80000) {
        wldcore_menu_push_screen_transition_level(0x2B, 0, 1);
    } else if (flags & 0x20000000) {
        wldcore_menu_push_screen_transition_level(0x1C, 0, 0);
    }
}
