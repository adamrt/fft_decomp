#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

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
    s32 mode;
    s32 value;
    s32 i;
    s32 unused[4];

    mode = level->mode;
    if (mode == 1) {
        if (world_thread_is_running(0xC) == 0) {
            g_wldcore_menu_result = 0;
            g_main_system_flags &= -5;
            g_wldcore_menu_stack_depth--;
            wldcore_list_open_available_propositions();
        }
        return;
    }
    if (mode == 6) {
        if (world_thread_is_running(0xC) == 0) {
            g_wldcore_menu_result = 0;
            g_main_system_flags &= -5;
            g_wldcore_menu_stack_depth--;
            wldcore_proposition_push_detail_level(g_wldcore_selected_proposition_row[0].fields.id - 1);
            wldcore_window_set_render_state_2_from_arg_0x04((s32*)level);
            wldcore_menu_push_number_panel_level();
        }
        return;
    }
    if ((u32)(mode - 3) < 2U || mode == 5) {
        wldcore_proposition_step_send_unit_browser(level);
        return;
    }
    if (mode == 2) {
        wldcore_proposition_step_send_unit_slot_input(level);
        return;
    }

    if (!(g_wldcore_window_render_records[level->layout_render].flags & 0x100)) {
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF;
        if (value != level->shown_preview) {
            level->shown_preview = value;
            wldcore_window_load_party_unit_sprite_record(level, value, level->preview_render);
        }
    }

    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_set_states_and_blue_tint((wldcore_window_tint_request_t*)level);
        wldcore_menu_push_message_level(0x109A, 1);
        return;
    }
    if (buttons & PSX_PAD_START) {
        if (g_wldcore_proposition_send_unit_count == 0) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            return;
        }
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->mode = 6;
        g_wldcore_context_value_display_mode = 4;
        g_wldcore_displayed_numeric_value_secondary = g_wldcore_proposition_gil_amount;
        /* Shared tail: a duplicated copy is scheduled differently and
         * only partly cross-jumped. */
        goto suspend;
    }
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->mode = 1;
    suspend:
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_wldcore_window_record_count -= 7;
        g_wldcore_window_render_record_count -= 5;
        g_wldcore_window_render_object_count -= 0xC;
        g_main_system_flags |= 4;
        return;
    }
    if (buttons & PSX_PAD_SQUARE) {
        world_thread_set_parameters(0xC, 0, 0, 1);
        world_thread_set_parameters(0xE, 0, -1, 0);
        g_wldcore_window_records[level->list_window].flags |= 0x10;
        g_wldcore_window_render_records[level->layout_render].flags |= 0x10;
        g_wldcore_window_records[level->frame_window_a].flags |= 0x10;
        g_wldcore_window_records[level->frame_window_b].flags |= 0x10;
        g_wldcore_window_render_records[level->preview_render].flags |= 0x10;
        g_wldcore_window_render_records[level->list_render].flags |= 0x10;
        g_main_system_flags |= 0x800;
        level->mode = 3;
        return;
    }
    if (!(buttons & PSX_PAD_CIRCLE)) {
        return;
    }

    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF;
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        if (g_wldcore_proposition_send_units[i] == value) {
            for (value = i + 1; value < 3; value++) {
                g_wldcore_proposition_send_units[value - 1] = g_wldcore_proposition_send_units[value];
            }
            g_wldcore_proposition_send_unit_count--;
            wldcore_window_draw_text_rows(level);
            wldcore_proposition_load_send_unit_candidates(level);
            world_thread_request_redraw(0xC);
            return;
        }
    }

    if (g_wldcore_proposition_send_unit_count == 3) {
        g_wldcore_window_records[level->frame_window_a].palette
            = g_wldcore_window_records[level->frame_window_b].palette = 0xA;
        g_wldcore_window_render_records[level->portrait_frame].flags |= 0x100;
        g_wldcore_window_render_records[level->portrait_frame].flags ^= 0x10;
        g_wldcore_menu_result = -1;
        g_wldcore_window_records[level->portrait_window].flags ^= 0x10;
        level->shown_unit = -1;
        level->mode = 2;
        level->slot = 0;
        g_wldcore_window_records[level->cursor_window].flags &= ~0x10;
        g_wldcore_window_records[level->cursor_window].x
            = g_wldcore_window_render_records[level->layout_render].base_x + 6;
        g_wldcore_window_records[level->cursor_window].y
            = g_wldcore_window_render_records[level->layout_render].base_y - level->slot * -16 + 0xC;
        return;
    }
    g_wldcore_proposition_send_units[g_wldcore_proposition_send_unit_count] = value;
    g_wldcore_proposition_send_unit_count++;

    wldcore_window_draw_text_rows(level);
    wldcore_proposition_load_send_unit_candidates(level);
    world_thread_request_redraw(0xC);
}
