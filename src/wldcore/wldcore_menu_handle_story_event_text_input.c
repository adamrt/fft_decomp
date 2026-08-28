#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

void wldcore_list_open_story_events(void);

/* Handle confirm/cancel input for the story-event list level. */
void wldcore_menu_handle_story_event_text_input(s32* selection) {
    s32 depth;
    s32 index;
    s32 value;
    u32 flags;

    if ((g_wldcore_window_render_records[g_wldcore_scroll_text_render_record_index].flags & 0x100) != 0) {
        return;
    }
    if ((g_main_system_flags & 4) && world_thread_is_running(0xE) == 0) {
        flags = g_main_system_flags;
        if ((flags & 8) == 0) {
            value = *selection;
            g_main_saved_list_cursor_state = g_wldcore_story_events_cursor_state;
            g_main_system_flags = (flags ^ 1) | 0x40000;
            g_main_replay_story_event_index = value;
            g_main_saved_weather_variable = world_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
            return;
        }
    }
    if ((g_wldcore_new_button_presses & PSX_PAD_TRIANGLE) != 0) {
        world_script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
        world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_story_event_ids[*selection]);
        g_main_system_flags |= 4;
        wldcore_fade_start_screen(2, 0x10);
        world_thread_set_parameters(0xE, 0, -1, 0);
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_sound_enqueue_audio_command(2, 0x10);
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CANCEL);
    depth = g_wldcore_menu_stack_depth;
    index = depth - 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags |= 0x100;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags &= ~0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags &= ~0x10;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags &= ~0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.upper_window].flags &= ~0x10;
    g_wldcore_menu_stack_depth = depth - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.lower_window].flags &= ~0x10;
    wldcore_list_open_story_events();
}
