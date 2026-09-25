#include "fft/wldcore.h"
#include "psx/pad.h"

/* Step handler for the save confirmation level.
 *
 * Drives the screen dim: fade_in raises the overlay colour in steps of 8 and
 * fade_out lowers it again, popping the level when it reaches 0. While no fade
 * runs it takes input: button 0x100 opens the help text for the highlighted
 * row through g_wldcore_sound_novel_quit_help_text_ids, up/down toggle the two rows and move the cursor
 * window, button 0x20 on row 0 starts the save (system flag 4, audio command
 * 2/0x20, main_sound_stop_weather_sfx_music), and button 0x20 on row 1 or
 * button 0x40 begins the fade out. Once the save finishes (WORLD thread 14
 * idle) it shifts the operand history g_wldcore_sound_novel_text_history_0..66, captures the play time
 * and hands off to wldcore_menu_push_screen_transition_level with screen 0x1b.
 *
 * Every colour store rereads the fade word through a local: a direct
 * `u8 = level->fade_in` assignment narrows the reload to `lbu`, while the
 * target keeps `lw`. The target stores the -1 history marker with a signed
 * `addiu`, so it is written through an s16 view of the u16 slot. The cursor
 * update needs the window pointer, the base_y local and a separate offset
 * local: that keeps base_y as the first `addu` operand and stops fold from
 * reassociating the +0xe. */
void wldcore_menu_step_sound_novel_quit_level(wldcore_menu_save_confirm_level_t* level) {
    u32 flags;
    u32 buttons;
    s32 value;
    s32 fade;
    s32 base;
    s32 offset;
    wldcore_window_record_t* record;
    s32 window;
    u16 first;

    if (level->fade_in < 0x60) {
        level->fade_in += 8;
        fade = level->fade_in;
        g_wldcore_screen_fade_state.boxes[0].r = fade;
        fade = level->fade_in;
        g_wldcore_screen_fade_state.boxes[0].g = fade;
        fade = level->fade_in;
        g_wldcore_screen_fade_state.boxes[0].b = fade;
    }

    if (level->fade_out != 0) {
        level->fade_out -= 8;
        fade = level->fade_out;
        g_wldcore_screen_fade_state.boxes[0].r = fade;
        fade = level->fade_out;
        g_wldcore_screen_fade_state.boxes[0].g = fade;
        fade = level->fade_out;
        g_wldcore_screen_fade_state.boxes[0].b = fade;
        if (level->fade_out <= 0) {
            g_wldcore_menu_result = 0x1C0;
            g_wldcore_screen_fade_state.flags[0] ^= 8;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth--;
            wldcore_menu_dispatch_resume_handler();
        }
        return;
    }

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    flags = g_main_system_flags;
    if (flags & 8) {
        return;
    }

    if (flags & 4) {
        if (level->fade_in < 0xFF) {
            level->fade_in += 8;
            if (level->fade_in >= 0x100) {
                level->fade_in = 0xFF;
            }
            fade = level->fade_in;
            g_wldcore_screen_fade_state.boxes[0].r = fade;
            fade = level->fade_in;
            g_wldcore_screen_fade_state.boxes[0].g = fade;
            fade = level->fade_in;
            g_wldcore_screen_fade_state.boxes[0].b = fade;
            return;
        }
        if (world_thread_is_running(0xE) != 0) {
            return;
        }
        g_wldcore_screen_brightness_rgb[0] = 0;
        g_wldcore_screen_brightness_rgb[1] = 0;
        g_wldcore_screen_brightness_rgb[2] = 0;
        g_wldcore_screen_fade_state.flags[0] |= 2;
        first = g_wldcore_sound_novel_text_history_0;
        *(s16*)&g_wldcore_sound_novel_text_history_0 = -1;
        g_wldcore_menu_result = 0;
        g_wldcore_sound_novel_countdown_frames = 0;
        g_main_system_flags ^= 4;
        g_wldcore_sound_novel_text_history_3 = g_wldcore_sound_novel_text_history_2;
        g_wldcore_sound_novel_text_history_2 = g_wldcore_sound_novel_text_history_1;
        g_wldcore_sound_novel_text_history_1 = first;
        wldcore_capture_state_and_play_time();
        *(s32*)&g_wldcore_screen_fade_state.priorities[0] = 0xF;
        g_wldcore_screen_fade_state.flags[1] = 0;
        g_wldcore_window_record_count -= 3;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 2;
        main_sound_remove_vfx_resource(g_wldcore_sound_novel_sound_resource);
        wldcore_menu_push_screen_transition_level(
            0x1B, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.selected_entry, 0);
        return;
    }

    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->fade_out = level->fade_in;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_reset_slot_state(&level->cursor_window);
        wldcore_menu_push_text_window_level(g_wldcore_sound_novel_quit_help_text_ids[level->choice] | 0x1000, 1);
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0
        || wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        value = level->choice ^ 1;
        level->choice = value;
        record = &g_wldcore_window_records[level->cursor_window];
        base = g_wldcore_window_render_records[level->render_index].base_y;
        offset = value * 0x10 + 0xE;
        record->y = base + offset;
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->cursor_window].sequence = 1;
        window = level->cursor_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        if (level->choice == 0) {
            g_main_system_flags |= 4;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            if (world_thread_is_running(0xE) != 0) {
                world_thread_set_task_id_to_three(0xE);
            }
            wldcore_sound_enqueue_audio_command(2, 0x20);
            main_sound_stop_weather_sfx_music();
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            return;
        }
        level->fade_out = level->fade_in;
    }
}
