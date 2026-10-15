/* Sound novel, quit confirmation and history levels. */

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Reload the saved background, restore the sound-novel windows when needed,
 * and resume the message thread. */
void wldcore_window_reload_data_set_and_resume_thread14(wldcore_menu_stack_record_t* level) {
    s16 set = g_wldcore_active_saved_record.background_set;

    if (set != -1) {
        wldcore_gfx_load_background_set(set);
        file_wait_for_pending_variant();
    }
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags
        &= ~WORLD_RENDER_RECORD_BLANK;
    if (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE) {
        g_wldcore_window_records[level->sound_novel.left_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->sound_novel.right_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    thread_resume(WLDCORE_THREAD_SLOT_MESSAGE);
}

/* The g_wldcore_menu_publish_handlers entry of WLDCORE_MENU_LEVEL_SOUND_NOVEL: nothing to publish. */
void wldcore_menu_publish_sound_novel_level(void) {
}

/*
 * Per-frame handler for a WLDCORE brightness/fade menu level.
 *
 * The unused array reproduces the target's 0x40-byte frame. The 0x140 state-flag test is a
 * 32-bit load (`lw`) of the u16 g_wldcore_active_saved_record.state_flags in the target, so it
 * reads the flags through an s32 cast; the u16 declaration gives `lhu`.
 */
void wldcore_menu_step_sound_novel_level(wldcore_menu_sound_novel_level_t* level) {
    s32 unused[2];
    u16* state_flags;
    wldcore_sortbox_state_t* fade;
    s16* brightness;
    u32 buttons;
    s32 width;
    s32 window;
    u16 shade;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && !(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
        switch (level->phase) {
        case WLDCORE_SOUND_NOVEL_PHASE_OPEN_HISTORY:
            if (level->countdown == 0) {
                g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                wldcore_menu_push_sound_novel_history_level();
                return;
            }
            level->countdown--;
            return;
        case WLDCORE_SOUND_NOVEL_PHASE_END:
            if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
                return;
            }
            g_menu_blocked_buttons = 0;
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            wldcore_gfx_prepare_play_time_display();
            fade = &g_wldcore_screen_fade_state;
            g_wldcore_screen_fade_state.priorities[0] = 0xF;
            g_wldcore_screen_fade_state.flags[1] = 0;
            g_wldcore_window_record_count -= 3;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 5;
            g_wldcore_menu_stack_depth--;
            fade->flags[0] = (fade->flags[0] & ~WLDCORE_SCREEN_FADE_BOX_VISIBLE) | WLDCORE_SCREEN_FADE_OUT;
            main_sound_stop_weather_sfx_music();
            suzuki_sound_remove_resource(g_wldcore_sound_novel_sound_resource);
            wldcore_reset_selected_saved_record(g_wldcore_saved_record_index);
            g_wldcore_sound_novel_countdown_frames = 0;
            wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TREASURE_DETAIL, level->message, 0);
            return;
        }
    }

    state_flags = &g_wldcore_active_saved_record.state_flags;
    if (*state_flags & WLDCORE_OPCODE_STATE_END) {
        return;
    }

    if (wldcore_script_run_until_yield() != 0) {
        if (g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1)) {
            if (g_input_current_buttons & PSX_PAD_L1) {
                if (g_wldcore_active_saved_record.brightness < 0xFF) {
                    g_wldcore_active_saved_record.brightness++;
                }
            }
            if (g_input_current_buttons & PSX_PAD_R1) {
                brightness = &g_wldcore_active_saved_record.brightness;
                if (*brightness != 0) {
                    (*brightness)--;
                }
            }
            shade = g_wldcore_active_saved_record.brightness;
            g_wldcore_screen_fade_state.boxes[1].r = shade;
            g_wldcore_screen_fade_state.boxes[1].g = shade;
            g_wldcore_screen_fade_state.boxes[1].b = shade;
            level->fade_timer = 0x3C;
        }

        if (level->fade_timer != 0) {
            g_wldcore_window_render_records[level->render_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[level->overlay_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            width = 0x100 - g_wldcore_active_saved_record.brightness;
            g_wldcore_window_render_records[level->render_index].bounds.w = width / 4;
            level->fade_timer--;
        } else {
            g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[level->overlay_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
        }

        if (world_menu_get_event_state_flag() != 0) {
            buttons = g_wldcore_input_new_button_presses;
            if (buttons & PSX_PAD_CIRCLE) {
                return;
            }
            g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
                = 0;
            if (buttons & PSX_PAD_SELECT) {
                wldcore_menu_push_message_level(0x1036, 1);
                return;
            }
            if ((buttons & PSX_PAD_START)
                && (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE)
                && !(g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_WHITE_OVERLAY)) {
                g_wldcore_window_records[level->left_window].sequence
                    = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON_PRESS;
                g_wldcore_window_records[level->left_window].frame_index
                    = g_wldcore_window_records[level->left_window].anim_counter = 0;
                level->fade_timer = 0;
                g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
                g_wldcore_window_records[level->overlay_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
                wldcore_menu_push_sound_novel_quit_level();
                return;
            }
            if ((g_input_current_buttons & PSX_PAD_SQUARE)
                && (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE)
                && g_wldcore_active_saved_record.text_history_3 != -1) {
                g_wldcore_window_records[level->right_window].sequence
                    = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON_PRESS;
                window = level->right_window;
                g_wldcore_window_records[window].anim_counter = 0;
                g_wldcore_window_records[window].frame_index = 0;
                level->fade_timer = 0;
                g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
                g_wldcore_window_records[level->overlay_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
                g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                level->phase = WLDCORE_SOUND_NOVEL_PHASE_OPEN_HISTORY;
                level->countdown = 8;
            }
        } else {
            g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
                = 0xA;
        }
    } else {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            world_thread_request_stop(WLDCORE_THREAD_SLOT_MESSAGE);
        }
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x20);
        main_sound_stop_weather_sfx_music();
        level->phase = WLDCORE_SOUND_NOVEL_PHASE_END;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        if (!(*state_flags & WLDCORE_OPCODE_STATE_WHITE_OVERLAY)) {
            wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        }
    }
}

/* Pushes the type-0x16 menu level: two window records (sequences 2 and 4)
 * and one render record showing text 0xb83a in a 0x3c x 0x30 image at
 * (-0x1e, -0x18), with the windows offset inside it. Blocks every button
 * (-1), clears the fade box color, sets fade flag 8 and clears the new
 * level's 32-bit values 0x10..0x1c.
 *
 * The level is written through the save_confirm member of the stack-record
 * union; union field access keeps the target's per-field `$at` bases. The
 * fade state is written through a pointer, which keeps the
 * target's register base for the offset-0 32-bit flags. The unused array
 * preserves the target's 0x68-byte frame. */
void wldcore_menu_push_sound_novel_quit_level(void) {
    RECT bounds;
    u8 unused[0x28];
    point32_t* base;
    s32 index;
    s32 depth;
    wldcore_sortbox_state_t* fade;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.second_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x3C;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record_variant(index, bounds, 0, WLDCORE_TEXT_ID_QUIT_CONTINUE, g_wldcore_window_image_buffer);
    base = &g_wldcore_window_render_records[index].base;
    g_wldcore_window_render_records[index].priority = 8;
    depth = g_wldcore_menu_stack_depth;
    base->x = -(bounds.w / 2);
    base->y = -0x18;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.second_window].position.x
        = g_wldcore_window_render_records[index].base.x + 4;
    g_menu_blocked_buttons = -1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.second_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_menu_stack_depth = depth + 1;
    fade = &g_wldcore_screen_fade_state;
    fade->boxes[0].r = 0;
    fade->boxes[0].g = 0;
    fade->boxes[0].b = 0;
    fade->flags[0] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
    g_wldcore_menu_stack_records_next[depth].save_confirm.choice = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm._unknown_18 = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm.fade_in = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm.fade_out = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_QUIT;
}

/* Restore the save confirmation palettes and cursor animation. */
void wldcore_window_clear_slot_state(wldcore_menu_stack_record_t* level) {
    s32 first = level->save_confirm.cursor_window;
    s32 second = level->save_confirm.second_window;
    s32 index;
    /* The original frame reserves eight unused bytes. */
    s32 scratch[2];

    g_wldcore_window_render_records[level->save_confirm.render_index].palette = 0;
    g_wldcore_window_records[second].palette = 0;
    g_wldcore_window_records[first].palette = 0;
    g_wldcore_window_records[level->save_confirm.cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    index = level->save_confirm.cursor_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}

/* Restore the save confirmation windows and restart the cursor animation. */
void wldcore_window_reset_slot_state(wldcore_menu_save_confirm_level_t* level) {
    s32 index;

    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_records[level->second_window].palette = 6;
    g_wldcore_window_render_records[level->render_index].palette = 2;
    g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    index = level->cursor_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}

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

/* Restore the HUD position and display the current play time.
 * Cache the flags and time before publishing the display fields. */
void wldcore_gfx_prepare_play_time_display(void) {
    u32 flags;
    s32 hours;
    s32 minutes;
    s32 seconds;

    g_wldcore_context_value_display_position = g_wldcore_saved_context_value_display_position;
    g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_PLAY_TIME;
    g_wldcore_hud_ot_priority = 1;
    flags = g_main_system_flags;
    hours = g_main_system_play_time_hours;
    minutes = g_main_system_play_time_minutes;
    seconds = g_main_system_play_time_seconds;
    g_main_system_flags = flags | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_displayed_numeric_value = hours;
    g_wldcore_displayed_numeric_value_secondary = minutes;
    g_wldcore_displayed_numeric_value_tertiary = seconds;
}

/* Pushes the save-slot selection level (menu type 0x17).
 *
 * Suspends WORLD thread 14, appends the two header windows (kinds 0x6c and
 * 0x6d, the second hidden) and the 0xf8 x 0xb0 panel render record built from
 * g_wldcore_window_image_buffer, centers the newest history entry's name text
 * (0x8800 + g_wldcore_active_saved_record.text_history_3) in it, and copies the four operand-history entries
 * (newest first) into the level at +0x0c..+0x18. It also tints the picture
 * render record to half brightness, clears the screen dim and sets the menu
 * result to -1. The first header window is hidden when the second history
 * entry is empty.
 *
 * The level is written through the sound_novel_history member of the
 * stack-record union (as in wldcore_menu_push_sound_novel_quit_level).
 *
 * Several spellings are load-bearing. `windows` holds the record base in a
 * saved register: the window positions are derived from it and the second
 * window's anim_counter store goes through it, while the other field stores use
 * the absolute `$at` form. The render position and color pointers reproduce
 * the target's shared render-record base. The history is read through a
 * pointer to its newest entry: the indexed reads count as in-struct
 * references, so the scheduler keeps each load after the preceding level
 * store. `top` and `depth` are separate locals for the target's register
 * assignment, and `unused` preserves the 0x50-byte frame. */
void wldcore_menu_push_sound_novel_history_level(void) {
    RECT bounds;
    u8 unused[8];
    world_animated_render_record_t* windows;
    wldcore_window_xy_t* positions;
    point32_t* position;
    CVECTOR* colors;
    s16* newest;
    s32 index;
    s32 depth;
    s32 top;

    thread_suspend(WLDCORE_THREAD_SLOT_MESSAGE);

    windows = g_wldcore_window_records;
    positions = (wldcore_window_xy_t*)&windows->position.x;
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.upper_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_UPPER;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    positions[index].x = 0;
    positions[index].y = -0x6A;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.lower_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_LOWER;
    g_wldcore_window_records[index].priority = 9;
    windows[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    positions[index].x = 0;
    positions[index].y = 0x5E;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.render_index = index;
    bounds.w = 0xF8;
    bounds.h = 0xB0;
    bounds.x = 0;
    bounds.y = 0;
    wldcore_window_build_render_record_image(index, bounds, 1, g_wldcore_window_image_buffer);
    position = &g_wldcore_window_render_records[index].base;
    newest = &g_wldcore_active_saved_record.text_history_3;
    g_wldcore_window_render_records[index].priority = g_wldcore_menu_ordering_table_offset;
    position->x = -0x80;
    position->y = -0x64;
    g_wldcore_window_render_records[index].clut.x = 0x3C0;
    g_wldcore_window_render_records[index].clut.y = 0x1F2;
    wldcore_window_draw_centered_text(index, *newest + TEXT_ID_WORLD_MESSAGE_BASE);

    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_pair_render.first_window]
        .palette = 0xA;
    /* Direct record.color access changes the column base and instruction scheduling. */
    colors = &g_wldcore_window_render_records->color;
    ((CVECTOR*)((u8*)colors
         + g_wldcore_active_saved_record.picture_render_index * sizeof(g_wldcore_window_render_records[0])))
        ->r = 0x80;
    ((CVECTOR*)((u8*)colors
         + g_wldcore_active_saved_record.picture_render_index * sizeof(g_wldcore_window_render_records[0])))
        ->g = 0x80;
    ((CVECTOR*)((u8*)colors
         + g_wldcore_active_saved_record.picture_render_index * sizeof(g_wldcore_window_render_records[0])))
        ->b = 0x80;
    g_wldcore_screen_fade_state.boxes[0].r = 0;
    g_wldcore_screen_fade_state.boxes[0].g = 0;
    g_wldcore_screen_fade_state.boxes[0].b = 0;
    g_wldcore_screen_fade_state.flags[0] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;

    top = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.scroll = 0;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.newest_text_id = *newest;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[0] = newest[-1];
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[1] = newest[-2];
    g_menu_blocked_buttons = -1;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[2] = newest[-3];
    if (g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[0] == -1) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[top].sound_novel_history.upper_window].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
    }

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].sound_novel_history.phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM;
    g_wldcore_menu_stack_records_next[depth].sound_novel_history.step = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_HISTORY;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Handler of the history-text level (type 0x17).
 *
 * Phase 1 steps the picture render record (g_wldcore_active_saved_record.picture_render_index) and g_wldcore_hud_color
 * from 0x80 down to 0x60 while the screen fade box brightens, in five steps. Phase 2 runs the reverse ramp and then, if
 * g_wldcore_active_saved_record.text_id names a text, clears the image buffer for the level's render record (width *
 * height / 8 32-bit values), renders text 0x8800 + g_wldcore_active_saved_record.text_id into it, uploads it and clears
 * script variables 0x5c-0x5e; it then pops the level and resumes WORLD thread 14. Phase 0 scrolls the text history with
 * up/down, hiding the arrow window at either end, and starts phase 2 once button bit 0x80 is released.
 *
 * Load-bearing spellings: the nonzero phases are a switch nested under
 * `phase != 0`, matching the target's test order; each fade case holds
 * &g_wldcore_active_saved_record.picture_render_index in its own pointer local, and `shade` is shared between the
 * cases (a global pseudo, as in the target's $a2); the text box position is
 * read into `x`/`y` so the s32 point is loaded with `lw`; `unused` preserves
 * the 0x58-byte frame. */
void wldcore_menu_step_sound_novel_history_level(wldcore_menu_sound_novel_history_level_t* level) {
    glyph_blit_t box;
    point32_t point;
    u8 unused[0x20];
    u32* image;
    s16* index1;
    s16* index2;
    s32 shade;
    s32 i;
    s32 text_id;
    s32 step;
    s32 depth;
    s32 render_index;
    s32 x;
    s32 y;

    if (level->phase != WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE) {
        switch (level->phase) {
        case WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM:
            shade = 0x80 - level->step * 8;
            index1 = &g_wldcore_active_saved_record.picture_render_index;
            g_wldcore_window_render_records[*index1].color.r = shade;
            g_wldcore_window_render_records[*index1].color.g = shade;
            g_wldcore_window_render_records[*index1].color.b = shade;
            g_wldcore_hud_color.r = shade;
            g_wldcore_hud_color.g = shade;
            g_wldcore_hud_color.b = shade;
            shade = level->step * 8;
            g_wldcore_screen_fade_state.boxes[0].r = shade;
            g_wldcore_screen_fade_state.boxes[0].g = shade;
            g_wldcore_screen_fade_state.boxes[0].b = shade;
            step = level->step + 1;
            level->step = step;
            if (step >= 5) {
                level->step = 0;
                level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            }
            break;
        case WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE:
            index2 = &g_wldcore_active_saved_record.picture_render_index;
            shade = level->step * 0x10 + 0x40;
            g_wldcore_window_render_records[*index2].color.r = shade;
            g_wldcore_window_render_records[*index2].color.g = shade;
            g_wldcore_window_render_records[*index2].color.b = shade;
            g_wldcore_hud_color.r = shade;
            g_wldcore_hud_color.g = shade;
            g_wldcore_hud_color.b = shade;
            shade = 0x20 - level->step * 8;
            g_wldcore_screen_fade_state.boxes[0].r = shade;
            g_wldcore_screen_fade_state.boxes[0].g = shade;
            g_wldcore_screen_fade_state.boxes[0].b = shade;
            step = level->step + 1;
            level->step = step;
            if (step < 5) {
                break;
            }
            text_id = g_wldcore_active_saved_record.text_id;
            if (text_id != -1) {
                text_id += TEXT_ID_WORLD_MESSAGE_BASE;
                i = 0;
                image = g_wldcore_window_image_buffer;
                point.x = 8;
                point.y = 8;
                for (;;) {
                    render_index = level->render_index;
                    if (i >= g_wldcore_window_render_records[render_index].bounds.w
                            * g_wldcore_window_render_records[render_index].bounds.h / 8) {
                        break;
                    }
                    i++;
                    *image = 0;
                    image++;
                }
                x = point.x;
                y = point.y;
                box.rect.x = x;
                box.rect.y = y;
                box.stride = g_wldcore_window_render_records[level->render_index].bounds.w;
                menu_display_text_entry_past_choice_marks(text_id, g_wldcore_window_image_buffer, &box);
                wldcore_window_load_image_record_to_vram(level->render_index, g_wldcore_window_image_buffer);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
            }
            level->step = 0;
            level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_wldcore_screen_fade_state.flags[0] ^= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth = depth - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window]
                .palette = 0;
            thread_resume(WLDCORE_THREAD_SLOT_MESSAGE);
            break;
        }
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->scroll < 3 && level->text_ids[level->scroll] != -1) {
            wldcore_window_draw_indexed_text(level, 4);
            level->scroll = level->scroll + 1;
            if (level->scroll == 3 || level->text_ids[level->scroll] == -1) {
                g_wldcore_window_records[level->upper_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
            } else {
                g_wldcore_window_records[level->upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            }
            g_wldcore_window_records[level->lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0 && level->scroll != 0) {
        wldcore_window_draw_indexed_text(level, 2);
        level->scroll = level->scroll - 1;
        if (level->scroll == 0) {
            g_wldcore_window_records[level->lower_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
        } else {
            g_wldcore_window_records[level->lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
        g_wldcore_window_records[level->upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE;
    }
}

/* Skip -1, otherwise draw the history text in text section 17 (0x8800). */
void wldcore_window_draw_indexed_text(wldcore_menu_sound_novel_history_level_t* level, s32 index) {
    /* Named text-array access reverses the final address-add operands. */
    s32 text_id = ((s32*)level)[index + level->scroll];

    if (text_id != -1) {
        wldcore_window_draw_centered_text(level->render_index, text_id + TEXT_ID_WORLD_MESSAGE_BASE);
    }
}
