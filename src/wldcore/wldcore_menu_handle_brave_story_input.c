#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

u32 wldcore_input_check_repeating_directional(u32 buttons);
void wldcore_window_set_list_window_states(wldcore_menu_list_window_level_t* state);
void wldcore_list_open_story_events(void);
void wldcore_list_open_unexplored_lands(void);
void wldcore_list_open_treasures(void);
void wldcore_menu_push_entry_window_level(void);
void wldcore_list_open_script_flags_01a4_01bb(void);
void wldcore_list_open_set_script_variables_03c0_03ff(void);
void wldcore_menu_push_proposition_category_level(void);

/* Input/update step of the world-map main menu level.
 *
 * Latches the play-time counters into the D_800D09Ax mirror, then runs one of
 * three behaviours depending on g_main_system_flags bit 0x4 (a battle/event
 * hand-off is in progress) and the level's own mode word:
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

    if (g_wldcore_window_render_records[level->content_render].flags & 0x100) {
        return;
    }

    flags = g_main_system_flags;
    if (flags & 4) {
        if (level->mode == 2 && !(flags & 8)) {
            level->mode = 3;
            g_main_system_flags = flags | 0x800;
            wldcore_gfx_clear_vram_and_scratch(1);
        }
        if (level->mode != 3) {
            return;
        }
        if (world_thread_is_running(0xE) != 0) {
            return;
        }
        if ((g_main_system_flags & 0x20000000) && g_wldcore_audio_queue.count != 0) {
            return;
        }
        g_main_system_flags ^= 4;
        wldcore_window_set_render_objects_visible(1);
        g_wldcore_menu_stack_depth--;
        g_main_system_flags &= ~0x3800;
        wldcore_menu_dispatch_publish_handler();
        if (g_main_system_flags & 0x20000000) {
            g_main_system_flags = (g_main_system_flags ^ 0x20000001) | 0x10000000;
            return;
        }
        wldcore_fade_start_screen(4, 0x10);
        wldcore_menu_push_entry_window_level();
        return;
    }

    if (level->mode == 1) {
        if (!(g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->mode = 2;
        world_thread_set_parameters(0xE, 0, -1, 0);
        g_main_system_flags |= 4;
        wldcore_fade_start_screen(2, 0x10);
        if (g_main_system_flags & 0x20000000) {
            wldcore_sound_enqueue_audio_command(2, 0x10);
            wldcore_sound_enqueue_audio_command(4, 0);
        } else {
            wldcore_sound_enqueue_music_start(0x11B);
        }
        g_wldcore_brave_story_saved_cursor[0] = 0;
        return;
    }

    if (g_wldcore_audio_queue.count != 0) {
        return;
    }

    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        world_thread_set_parameters(0xE, 0x19, 0xB84A, 0);
        level->mode = 1;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 6;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        level->mode = 2;
        wldcore_window_set_list_window_states(level);
        wldcore_menu_push_message_level(
            g_wldcore_brave_story_help_text_ids[level->entries[level->selected_entry]] | 0x1000, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_brave_story_saved_cursor[0] = level->selected_entry;
        switch (level->entries[level->selected_entry]) {
        case 0:
            wldcore_load_message_block_if_changed(0xB);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_script_flags_01a4_01bb();
            return;
        case 1:
            wldcore_load_message_block_if_changed(4);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_story_events();
            return;
        case 2:
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_set_script_variables_03c0_03ff();
            return;
        case 3:
            wldcore_load_message_block_if_changed(0xA);
            wldcore_window_set_list_window_states(level);
            wldcore_menu_push_proposition_category_level();
            return;
        case 4:
            wldcore_load_message_block_if_changed(2);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_unexplored_lands();
            return;
        case 5:
            wldcore_load_message_block_if_changed(3);
            wldcore_window_set_list_window_states(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_treasures();
            return;
        case 6:
            world_thread_set_parameters(0xE, 0x19, 0xB84A, 0);
            level->mode = 1;
            g_wldcore_window_record_count -= 4;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 6;
            return;
        }
        return;
    }

    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (level->selected_entry != 0) {
            g_wldcore_window_records[level->main_window].y -= 0x10;
            level->selected_entry = level->selected_entry - 1;
        } else {
            g_wldcore_window_records[level->main_window].y -= (level->entry_count - 1) * -16;
            level->selected_entry = level->entry_count - 1;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }

    if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].y += 0x10;
            level->selected_entry = level->selected_entry + 1;
        } else {
            g_wldcore_window_records[level->main_window].y -= (level->entry_count - 1) * 16;
            level->selected_entry = 0;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
}
