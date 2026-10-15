#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Location-browser step of the proposition send-unit level (modes 3-5,
 * entered from wldcore_menu_step_send_unit_level).
 *
 * Mode 3 waits for WORLD threads 12 and 14, shows the two arrow windows,
 * enters mode 4 and starts the background threads for the highlighted list
 * entry. Mode 4 is interactive: 0x60 hides the arrows, stops threads 8, 12
 * and 9 and enters mode 5; 0x100 parks both arrows in palette 10 and pushes a
 * layout-window level; right (8) and left (4) wrap the slot through
 * unit_count, repaint the arrow sprites 0x66-0x69, set the scroll direction in
 * thread 8's block and reload the entry's text. Mode 5 waits for threads 8,
 * 12 and 9, clears system flags 0x20000 and 0x800, restarts threads 14 and 12
 * and shows the six records hidden on entry.
 *
 * Twin of the participant browser wldcore_menu_handle_participant_detail_input; the chained stores and the
 * shared scroll tail follow it. */
void wldcore_proposition_step_send_unit_browser(wldcore_menu_send_unit_level_t* level) {
    u32 buttons;
    s32 selected;
    s32 direction;
    s16* menu_selected_index;

    if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_OPEN) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) != 0) {
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_wldcore_window_records[level->left_arrow].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->right_arrow].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        level->mode = WLDCORE_SEND_UNIT_MODE_STATUS;
        menu_selected_index = &g_wldcore_window_panel_render_state.selected_index;
        wldcore_start_core_background_threads(g_wldcore_list_entry_values[*menu_selected_index] & TEXT_ID_ENTRY_MASK);
        level->slot = *menu_selected_index;
    } else if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS) {
        buttons = g_wldcore_input_new_button_presses;
        if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_wldcore_window_records[level->left_arrow].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[level->right_arrow].flags |= WORLD_RENDER_RECORD_HIDDEN;
            level->mode = WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE;
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_SELECT) {
            g_wldcore_window_records[level->left_arrow].palette = g_wldcore_window_records[level->right_arrow].palette
                = 0xA;
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            wldcore_menu_push_layout_window_level();
            return;
        }
        if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_wldcore_window_records[level->left_arrow].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
            g_wldcore_window_records[level->right_arrow].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
            g_wldcore_window_records[level->left_arrow].frame_index
                = g_wldcore_window_records[level->left_arrow].anim_counter
                = g_wldcore_window_records[level->right_arrow].frame_index
                = g_wldcore_window_records[level->right_arrow].anim_counter = 0;
        }
        if (wldcore_input_update_repeat(PSX_PAD_R1) != 0
            && g_wldcore_window_records[level->left_arrow].sequence != WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS) {
            g_wldcore_window_records[level->right_arrow].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS;
            g_wldcore_window_records[level->right_arrow].frame_index
                = g_wldcore_window_records[level->right_arrow].anim_counter = 0;
            if (level->unit_count == 1) {
                /* Shared tail: duplicating the sound call per arm is only
                 * partly cross-jumped and leaves an extra copy. */
                goto done;
            }
            selected = level->slot + 1;
            if (selected >= level->unit_count) {
                level->slot = 0;
            } else {
                level->slot = selected;
            }
            direction = 1;
        } else {
            if (wldcore_input_update_repeat(PSX_PAD_L1) == 0
                || g_wldcore_window_records[level->right_arrow].sequence == WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS) {
                return;
            }
            g_wldcore_window_records[level->left_arrow].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS;
            g_wldcore_window_records[level->left_arrow].frame_index
                = g_wldcore_window_records[level->left_arrow].anim_counter = 0;
            if (level->unit_count == 1) {
                goto done;
            }
            if (level->slot != 0) {
                level->slot--;
            } else {
                level->slot = level->unit_count - 1;
            }
            direction = 2;
        }
        g_wldcore_thread8_params.redraw_request = direction;
        wldcore_unit_build_status_panel_data(g_wldcore_list_entry_values[level->slot] & TEXT_ID_ENTRY_MASK);
        g_wldcore_threadc_params.redraw_request = 1;
        g_wldcore_thread9_params.redraw_request = 1;
    done:
        wldcore_sound_play_effect(MAIN_SFX_PAGE_SWITCH);
    } else if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE) {
        if (thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_EDITOR) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) != 0) {
            return;
        }
        level->mode = WLDCORE_SEND_UNIT_MODE_SELECT;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS);
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_UNITS, 0);
        thread_start(WLDCORE_THREAD_SLOT_WINDOW_PANEL, world_screen_build_at_list);
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, (s32)&g_wldcore_window_panel_render_state, 0, 0);
        g_wldcore_window_records[level->list_frame.window_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_frame.render_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_a].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_b].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->preview_render].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_render].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    }
}

/* Clears the palette of two window records and two render records, resets one
 * render record's color to neutral gray, then clears two more windows. */
void wldcore_window_clear_states_and_restore_tint(wldcore_menu_send_unit_level_t* level) {
    CVECTOR* color;

    g_wldcore_window_records[level->list_frame.window_index].palette
        = g_wldcore_window_render_records[level->list_frame.render_index].palette
        = g_wldcore_window_render_records[level->list_render].palette
        = g_wldcore_window_records[level->cursor_window].palette = 0;
    /* Each channel store reloads the level's render index. */
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->r = 0x80;
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->g = 0x80;
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->b = 0x80;
    g_wldcore_window_records[level->left_arrow].palette = g_wldcore_window_records[level->right_arrow].palette = 0;
}

/* Counterpart of 0x80076744: sets the two window states to 6 and 10, both
 * render record states to 2, and the render record color to 0x40/0x50/0x70. */
void wldcore_window_set_states_and_blue_tint(wldcore_menu_send_unit_level_t* level) {
    s32 pad[4];

    g_wldcore_window_records[level->list_frame.window_index].palette = 6;
    g_wldcore_window_render_records[level->list_frame.render_index].palette = 2;
    g_wldcore_window_render_records[level->list_render].palette = 2;
    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_render_records[level->preview_render].color.r = 0x40;
    g_wldcore_window_render_records[level->preview_render].color.g = 0x50;
    g_wldcore_window_render_records[level->preview_render].color.b = 0x70;
}

/* Top-level step of the proposition send-unit menu level, dispatching on the
 * level's mode.
 *
 * Modes 1 and 6 wait for WORLD thread 12 to go idle and then pop the level,
 * mode 6 continuing into the proposition detail level and its number panel;
 * modes 3, 4 and 5 run wldcore_proposition_step_send_unit_browser and mode 2 runs
 * wldcore_proposition_step_send_unit_slot_input.
 *
 * Otherwise it is the unit-list step: while the list render record is visible
 * the highlighted row's unit id (g_wldcore_list_entry_values[cursor] & 0x7ff) is redrawn into
 * the preview record when it changes, then the new button presses are
 * handled - 0x100 opens help message 0x109a, 0x800 (with at least one chosen
 * unit) confirms and leaves through mode 6, 0x40 cancels through mode 1, 0x80
 * hides the six records and parks the level in mode 3, and 0x20 toggles the
 * highlighted unit in or out of the three g_wldcore_proposition_send_units slots (removing it
 * compacts the list, adding it when all three are full instead switches the
 * level into the slot-editing mode 2).
 *
 * The target reserves a 0x30-byte frame but only saves s0 and ra; the unused
 * 16-byte array reproduces that frame. The removal and addition paths each
 * end in their own redraw calls, which jump2 cross-jumps from the count
 * store onward. */
void wldcore_menu_step_send_unit_level(wldcore_menu_send_unit_level_t* level) {
    s32 buttons;
    s32 value;
    s32 i;
    s32 unused[4];

    if (level->mode == WLDCORE_SEND_UNIT_MODE_CANCEL) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
            g_menu_blocked_buttons = 0;
            g_main_system_flags &= ~MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_menu_stack_depth--;
            wldcore_list_open_available_propositions();
        }
        return;
    }
    if (level->mode == WLDCORE_SEND_UNIT_MODE_ACCEPT) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
            g_menu_blocked_buttons = 0;
            g_main_system_flags &= ~MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_menu_stack_depth--;
            wldcore_proposition_push_detail_level(g_wldcore_selected_proposition_row[0].id - 1);
            wldcore_window_set_render_palette_2(&level->list_frame);
            wldcore_menu_push_number_panel_level();
        }
        return;
    }
    if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_OPEN || level->mode == WLDCORE_SEND_UNIT_MODE_STATUS
        || level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE) {
        wldcore_proposition_step_send_unit_browser(level);
        return;
    }
    if (level->mode == WLDCORE_SEND_UNIT_MODE_REPLACE) {
        wldcore_proposition_step_send_unit_slot_input(level);
        return;
    }

    if (!(g_wldcore_window_render_records[level->list_frame.render_index].flags & WORLD_RENDER_RECORD_POP_IN)) {
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
        if (value != level->shown_preview) {
            level->shown_preview = value;
            wldcore_window_load_party_unit_sprite_record(level, value, level->preview_render);
        }
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_set_states_and_blue_tint(level);
        wldcore_menu_push_message_level(0x109A, 1);
        return;
    }
    if (buttons & PSX_PAD_START) {
        if (g_wldcore_proposition_send_unit_count == 0) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            return;
        }
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->mode = WLDCORE_SEND_UNIT_MODE_ACCEPT;
        g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_COST_AND_FUNDS;
        g_wldcore_displayed_numeric_value_secondary = g_wldcore_proposition_gil_amount;
        /* Shared tail: a duplicated copy is scheduled differently and
         * only partly cross-jumped. */
        goto suspend;
    }
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->mode = WLDCORE_SEND_UNIT_MODE_CANCEL;
    suspend:
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_wldcore_window_record_count -= 7;
        g_wldcore_window_render_record_count -= 5;
        g_wldcore_window_render_object_count -= 0xC;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        return;
    }
    if (buttons & PSX_PAD_SQUARE) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_wldcore_window_records[level->list_frame.window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_frame.render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_a].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_b].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->preview_render].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_render].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        level->mode = WLDCORE_SEND_UNIT_MODE_STATUS_OPEN;
        return;
    }
    if (!(buttons & PSX_PAD_CIRCLE)) {
        return;
    }

    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        if (g_wldcore_proposition_send_units[i] == value) {
            for (value = i + 1; value < 3; value++) {
                g_wldcore_proposition_send_units[value - 1] = g_wldcore_proposition_send_units[value];
            }
            g_wldcore_proposition_send_unit_count--;
            wldcore_window_draw_text_rows(level);
            wldcore_proposition_load_send_unit_candidates(level);
            world_thread_request_redraw(WLDCORE_THREAD_SLOT_WINDOW_PANEL);
            return;
        }
    }

    if (g_wldcore_proposition_send_unit_count == 3) {
        g_wldcore_window_records[level->frame_window_a].palette
            = g_wldcore_window_records[level->frame_window_b].palette = 0xA;
        g_wldcore_window_render_records[level->portrait_frame].flags |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_render_records[level->portrait_frame].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_menu_blocked_buttons = -1;
        g_wldcore_window_records[level->portrait_window].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        level->shown_unit = -1;
        level->mode = WLDCORE_SEND_UNIT_MODE_REPLACE;
        level->slot = 0;
        g_wldcore_window_records[level->cursor_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->cursor_window].position.x
            = g_wldcore_window_render_records[level->list_frame.render_index].base.x + 6;
        g_wldcore_window_records[level->cursor_window].position.y
            = g_wldcore_window_render_records[level->list_frame.render_index].base.y + level->slot * 16 + 0xC;
        return;
    }
    g_wldcore_proposition_send_units[g_wldcore_proposition_send_unit_count] = value;
    g_wldcore_proposition_send_unit_count++;

    wldcore_window_draw_text_rows(level);
    wldcore_proposition_load_send_unit_candidates(level);
    world_thread_request_redraw(WLDCORE_THREAD_SLOT_WINDOW_PANEL);
}
