#include "fft/script.h"
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

    flags = g_main_system_flags & ~MAIN_SYSTEM_FLAG_RESTART;
    g_main_system_flags = flags;
    if ((flags
            & (MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY | MAIN_SYSTEM_FLAG_TITLE_TUTORIAL | MAIN_SYSTEM_FLAG_TUTORIAL_EVENT
                | MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY))
        == 0) {
        if (wldcore_script_process_conditional_set(
                g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_TEXT)
            != 0) {
            wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
            wldcore_menu_push_scrollable_text_window_level(
                g_wldcore_script_state.args[0] + TEXT_ID_WORLD_HELP_MESSAGE_BASE);
        } else {
            wldcore_menu_dispatch_pending_script_requests();
        }
        wldcore_sound_enqueue_audio_command(
            WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 4);
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY) {
        g_main_system_flags = flags ^ MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_main_saved_weather_variable);
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_STORY_EVENTS, 0, 0);
    } else if (flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) {
        g_main_system_flags = flags ^ MAIN_SYSTEM_FLAG_TUTORIAL_EVENT;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_main_saved_weather_variable);
        g_wldcore_audio_queue.music.tracks[0] = 0x1B;
        g_wldcore_audio_queue.music.tracks[1] = 0x1A;
        g_wldcore_audio_queue.current_slot = 2;
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES, 1, 0);
    } else if (flags & MAIN_SYSTEM_FLAG_TITLE_TUTORIAL) {
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES, 0, 1);
    } else if (flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) {
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_BRAVE_STORY, 0, 0);
    }
}
