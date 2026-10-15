#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Handle confirm/cancel input for the story-event list level. */
void wldcore_menu_handle_story_event_text_input(wldcore_menu_stack_record_t* level) {
    s32 depth;
    s32 index;
    s32 value;
    u32 flags;

    if ((g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
            & WORLD_RENDER_RECORD_POP_IN)
        != 0) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        flags = g_main_system_flags;
        if ((flags & MAIN_SYSTEM_FLAG_FADING) == 0) {
            value = level->story_event_index;
            g_main_saved_list_cursor_state = g_wldcore_story_events_cursor_state;
            g_main_system_flags = (flags ^ MAIN_SYSTEM_FLAG_RUNNING) | MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY;
            g_main_replay_story_event_index = value;
            g_main_saved_weather_variable = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
            return;
        }
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_TRIANGLE) != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_story_event_ids[level->story_event_index]);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CANCEL);
    depth = g_wldcore_menu_stack_depth;
    index = depth - 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_POP_IN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.upper_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.lower_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    wldcore_list_open_story_events();
}
