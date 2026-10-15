#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Step handler for the save confirmation level.
 *
 * Drives the screen dim: fade_in raises the overlay color in steps of 8 and
 * fade_out lowers it again, popping the level when it reaches 0. While no fade
 * runs it takes input: button 0x100 opens the help text for the highlighted
 * row through g_wldcore_sound_novel_quit_help_text_ids, up/down toggle the two rows and move the cursor
 * window, button 0x20 on row 0 starts the save (system flag 4, audio command
 * 2/0x20, main_sound_stop_weather_sfx_music), and button 0x20 on row 1 or
 * button 0x40 begins the fade out. Once the save finishes (WORLD thread 14
 * idle) it shifts the operand history g_wldcore_sound_novel_text_history_0..66, captures the play time
 * and hands off to wldcore_menu_push_screen_transition_level with screen 0x1b.
 *
 * Every color store rereads the 32-bit fade value through a local: a direct
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
    world_animated_render_record_t* record;
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
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_wldcore_screen_fade_state.flags[0] ^= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth--;
            wldcore_menu_dispatch_resume_handler();
        }
        return;
    }

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    flags = g_main_system_flags;
    if (flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }

    if (flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) {
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
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_wldcore_screen_brightness_rgb.r = 0;
        g_wldcore_screen_brightness_rgb.g = 0;
        g_wldcore_screen_brightness_rgb.b = 0;
        g_wldcore_screen_fade_state.flags[0] |= WLDCORE_SCREEN_FADE_OUT;
        first = g_wldcore_sound_novel_text_history_0;
        g_wldcore_sound_novel_text_history_0 = -1;
        g_menu_blocked_buttons = 0;
        g_wldcore_sound_novel_countdown_frames = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_sound_novel_text_history_3 = g_wldcore_sound_novel_text_history_2;
        g_wldcore_sound_novel_text_history_2 = g_wldcore_sound_novel_text_history_1;
        g_wldcore_sound_novel_text_history_1 = first;
        wldcore_gfx_prepare_play_time_display();
        g_wldcore_screen_fade_state.priorities[0] = 0xF;
        g_wldcore_screen_fade_state.flags[1] = 0;
        g_wldcore_window_record_count -= 3;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 2;
        suzuki_sound_remove_resource(g_wldcore_sound_novel_sound_resource);
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TREASURE_DETAIL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.selected_entry, 0);
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->fade_out = level->fade_in;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_reset_slot_state(level);
        wldcore_menu_push_text_window_level(g_wldcore_sound_novel_quit_help_text_ids[level->choice] | 0x1000, 1);
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        value = level->choice ^ 1;
        level->choice = value;
        record = &g_wldcore_window_records[level->cursor_window];
        base = g_wldcore_window_render_records[level->render_index].base.y;
        offset = value * 0x10 + 0xE;
        record->position.y = base + offset;
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        window = level->cursor_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        if (level->choice == 0) {
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
                world_thread_request_stop(WLDCORE_THREAD_SLOT_MESSAGE);
            }
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x20);
            main_sound_stop_weather_sfx_music();
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            return;
        }
        level->fade_out = level->fade_in;
    }
}
