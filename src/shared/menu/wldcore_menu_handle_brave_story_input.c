#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Input/update step of the world-map main menu level.
 *
 * Latches the play-time counters into the D_800D09Ax mirror, then runs one of
 * three behaviors depending on g_main_system_flags bit 0x4 (a battle/event
 * hand-off is in progress) and the level's own 32-bit mode:
 *  - hand-off in progress: wait for the world thread (slot 0xE) to stop and
 *    for the audio queue to drain, then tear the level down, pop the menu
 *    stack and either re-enter through the 0x10000000 path or fade out;
 *  - mode 1 (suspended): any of confirm/cancel resumes, refades in and
 *    restarts the world-map music;
 *  - otherwise: normal input.  Cancel (0x40) and menu entry 6 leave the world
 *    map through the 0xB84A world-thread entry point; 0x100 opens the help
 *    message for the highlighted row; confirm (0x20) dispatches the
 *    highlighted row through a seven-entry jump table at 0x800671f4; up/down
 *    move the cursor with wrap-around, stepping the window record's y by 16.
 *
 * The cancel path and menu entry 6 each spell out the window-count release;
 * jump2 cross-jumps them after the two count loads. The target reloads the
 * cursor after storing it to g_wldcore_brave_story_saved_cursor, which needs g_wldcore_brave_story_saved_cursor
 * accessed as an array element (a scalar store does not alias the level field). The wrap-to-bottom step is written `y
 * -= n * -16` to keep y as the first addu operand.
 */

void wldcore_menu_handle_brave_story_input(wldcore_menu_list_window_level_t* level) {
    u32 flags;
    u32 buttons;

    g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
    g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
    g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;

    if (g_wldcore_window_render_records[level->content_render].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }

    flags = g_main_system_flags;
    if (flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) {
        if (level->mode == WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP && !(flags & MAIN_SYSTEM_FLAG_FADING)) {
            level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT;
            g_main_system_flags = flags | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
            wldcore_gfx_clear_vram_and_scratch(1);
        }
        if (level->mode != WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT) {
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        if ((g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) && g_wldcore_audio_queue.count != 0) {
            return;
        }
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_window_set_render_objects_visible(1);
        g_wldcore_menu_stack_depth--;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
            | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER);
        wldcore_menu_dispatch_publish_handler();
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) {
            g_main_system_flags
                = (g_main_system_flags ^ (MAIN_SYSTEM_FLAG_RUNNING | MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY))
                | MAIN_SYSTEM_FLAG_RETURN_TO_TITLE;
            return;
        }
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_REDRAW_MAP, 0x10);
        wldcore_menu_push_entry_window_level();
        return;
    }

    if (level->mode == WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE) {
        if (!(g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->mode = WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) {
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 0);
        } else {
            wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
        }
        g_wldcore_brave_story_saved_cursor[0] = 0;
        return;
    }

    if (g_wldcore_audio_queue.count != 0) {
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_GOODBYE, 0);
        level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 6;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        level->mode = WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP;
        wldcore_window_set_list_window_states(level);
        wldcore_menu_push_message_level(
            g_wldcore_brave_story_help_text_ids[level->entries[level->selected_entry]] | 0x1000, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_brave_story_saved_cursor[0] = level->selected_entry;
        switch (level->entries[level->selected_entry]) {
        case WLDCORE_BRAVE_STORY_ROW_SCRIPT_FLAGS_01A4_01BB:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_SCRIPT_FLAGS_01A4_01BB);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_script_flags_01a4_01bb();
            return;
        case WLDCORE_BRAVE_STORY_ROW_STORY_EVENTS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_STORY_EVENTS);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_story_events();
            return;
        case WLDCORE_BRAVE_STORY_ROW_SET_SCRIPT_VARIABLES_03C0_03FF:
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_set_script_variables_03c0_03ff();
            return;
        case WLDCORE_BRAVE_STORY_ROW_PROPOSITIONS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_BRAVE_STORY_PROPOSITIONS);
            wldcore_window_set_list_window_states(level);
            wldcore_menu_push_proposition_category_level();
            return;
        case WLDCORE_BRAVE_STORY_ROW_UNEXPLORED_LANDS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_UNEXPLORED_LANDS);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_unexplored_lands();
            return;
        case WLDCORE_BRAVE_STORY_ROW_TREASURES:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_TREASURES);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_treasures();
            return;
        case WLDCORE_BRAVE_STORY_ROW_LEAVE:
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_GOODBYE, 0);
            level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
            g_wldcore_window_record_count -= 4;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 6;
            return;
        }
        return;
    }

    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->selected_entry != 0) {
            g_wldcore_window_records[level->main_window].position.y -= 0x10;
            level->selected_entry = level->selected_entry - 1;
        } else {
            g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * -16;
            level->selected_entry = level->entry_count - 1;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }

    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].position.y += 0x10;
            level->selected_entry = level->selected_entry + 1;
        } else {
            g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * 16;
            level->selected_entry = 0;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
}
