/* Rumor detail text level and the town's available-proposition list level with its handlers. */

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Opens the selected rumor in a scrollable text window and hides the parent
 * level's two windows and render record until the detail level closes.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one, hence case 1 before case 0 here.
 *
 * `unused` retains the target's 8 reserved frame bytes. */
void wldcore_bar_push_rumor_detail_level(s32 value) {
    s32 coordinate_mode = 0;
    s32 depth;
    u8 unused[8];

    g_text_substitution_values[0] = value + TEXT_ID_RUMOR_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RUMOR_ABOUT, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].rumor_detail.rumor_index = value;
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xDC;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 9;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.first_window
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.second_window
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.render_index
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_RUMOR_DETAIL;
}

/* Close this menu level once its message window is finished.
 *
 * Same wait as wldcore_menu_wait_text_then_pop_level; then toggles flag 0x10 on the
 * level's two windows and render record, pops the stack and marks the render
 * record 0x100 before wldcore_list_open_rumors redraws. */
void wldcore_bar_wait_rumor_text_then_pop_level(wldcore_menu_rumor_detail_level_t* level) {
    s32* index = &g_wldcore_scrollable_text_window.render_record_index;

    if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    g_wldcore_window_records[level->first_window].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[level->second_window].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[level->render_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth--;
    g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_POP_IN;
    wldcore_list_open_rumors();
}

/* Opens the town menu's proposition-location list (row kind 1 of
 * wldcore_bar_handle_menu_input). If the parent list level's frame is hidden (render flag
 * 0x10), it first unhides its three records, rebuilds it with wldcore_bar_build_menu_rows,
 * publishes text 0xb806 and resets its main window. It then lists the 96
 * propositions whose script variable 0x360 + i has neither bit 1 nor 2 set,
 * whose g_main_proposition_states entry is 0 in its low six bits and which
 * wldcore_proposition_is_available_at_location accepts for the current location
 * (entry values 0xd000 + i). With entries it shows text 0xb80b and up to six
 * rows at (0x4c, 0x50), width 0x96, and pushes a type-7 level; otherwise it
 * resumes the parent, shows text 0xb80d and marks the parent's content
 * published. */
void wldcore_list_open_available_propositions(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 count;
    s32 i;
    s32 index;
    world_animated_render_record_t* record;
    s32 record_index;

    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.frame_render]
            .flags
        & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .list_window.main_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .list_window.side_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.frame_render]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        wldcore_window_publish_slot_b806(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        index = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window;
        record = &g_wldcore_window_records[index];
        record->anim_counter = 0;
        record->frame_index = 0;
        g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.frame_render]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    count = 0;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (!(script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & (WLDCORE_PROPOSITION_VARIABLE_IN_PROGRESS | WLDCORE_PROPOSITION_VARIABLE_COMPLETED))
            && !(g_main_proposition_states[i] & WLDCORE_PROPOSITION_STATE_DAYS_MASK)
            && wldcore_proposition_is_available_at_location(i, g_wldcore_map_projection_state.marker.kind + 1) != 0) {
            g_wldcore_list_entry_values[count] = i + TEXT_ID_PROPOSITION_NAME_BASE;
            g_wldcore_list_row_flags[count] = 0;
            count++;
        }
    }
    if (count != 0) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_PROPOSITIONS_INTRO, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
        if (count < 6) {
            i = count;
        } else {
            i = 6;
        }
        layout.text_width = 0x96;
        origin.x = 0x4C;
        layout.visible_rows = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_AVAILABLE_PROPOSITIONS, count, i);
        wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    } else {
        wldcore_menu_dispatch_resume_handler();
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_NO_PROPOSITIONS, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.content_render = 1;
        return;
    }
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_AVAILABLE_PROPOSITIONS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Resume handler of the propositions list: block Circle, Cross and Select for the
 * levels underneath and show prompt 0xB80B again. */
void wldcore_list_propositions_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_PROPOSITIONS_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level runs. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_propositions_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Pop the propositions list level: toggle the input-blocked system flag and
 * unblock the menu buttons. */
/* Target 0x80072ef4. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_propositions_close_level(wldcore_menu_panel_level_t* level) {
    g_wldcore_menu_ordering_table_offset = 1;
    g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    --g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = 0;
}

/* List level input step for cursor slot 3 (help message 0x105f); confirmed
 * entries are handed to 0x800732b8. Confirm is refused with message 0xb80e
 * when g_main_save_proposition_count has reached 8. When script variable
 * 0x360 + entry has bit 8 set, the parent level's windows and render record
 * are hidden, message 0xb85c is shown and the level waits in state 2 for
 * confirm or cancel. */
void wldcore_list_handle_propositions_input(wldcore_menu_panel_level_t* level) {
    s32 depth;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        wldcore_list_propositions_close_level(level);
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_AVAILABLE_PROPOSITIONS);
            wldcore_proposition_push_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_AVAILABLE_PROPOSITIONS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (level->result == 2) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            level->result = 1;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        }
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_propositions_block_all_buttons(level);
        wldcore_menu_push_message_level(0x105F, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        if (g_main_save_proposition_count >= 8) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_NO_MORE_JOBS, 0);
            return;
        }
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        if (script_get_variable(
                (g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK)
                + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
            & WLDCORE_PROPOSITION_VARIABLE_ATTEMPTED) {
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window].flags
                |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.second_window]
                .flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2]
                                                .window_pair_render.render_index]
                .flags |= WORLD_RENDER_RECORD_HIDDEN;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TOLD_BEFORE, 0);
            level->result = 2;
            return;
        }
        level->result = 1;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}
