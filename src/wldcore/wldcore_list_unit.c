#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opens the list of active propositions at the current location as menu
 * level type 0xe.
 *
 * Restores the parent level's hidden window pair first. Starts WORLD thread
 * 14 on 0x8010f250 if needed with text 0xb819, then lists each proposition
 * whose flag 4 is set and whose location is g_wldcore_map_projection_state.marker.kind:
 * display value 0xd000 + proposition_id, clear row flags, source index in
 * entries. With no entries it rebuilds the parent window pair through
 * wldcore_bar_build_menu_rows and resumes the parent handler instead of pushing.
 * Twin of wldcore_list_open_propositions.
 *
 * The location is read through a pointer local: that keeps the load inside
 * the loop (the array stores may alias it) with only its address hoisted,
 * as in the target. The unused point record preserves the 24-byte local
 * area. */
void wldcore_list_open_completed_propositions(void) {
    point32_t origin;
    wldcore_window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 total;
    s32 previous;
    s32 loaded_count;
    s32* location;
    s32 entry_value_base;

    previous = g_wldcore_menu_stack_depth - 1;
    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags
        & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_RETURNED, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        location = &g_wldcore_map_projection_state.marker.kind;
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED)
                && g_main_active_propositions[i].location == *location) {
                g_wldcore_list_entry_values[count] = g_main_active_propositions[i].proposition_id + entry_value_base;
                g_wldcore_list_row_flags[count] = 0;
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }
    if (count != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count = count;
        if (count < 6) {
            i = count;
        } else {
            i = 6;
        }
        layout.text_width = 0x96;
        origin.x = 0x48;
        layout.visible_rows = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS, count, i);
        wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count, &layout,
            &origin);
    } else {
        wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS;
    g_wldcore_menu_stack_depth++;
}

/* Resume handler of the completed propositions list: block Circle, Cross and Select for the
 * levels underneath and show prompt 0xB819 again. */
void wldcore_list_completed_propositions_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_RETURNED, 0);
}

void wldcore_list_completed_propositions_block_all_buttons(wldcore_menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Handle input for the job-report confirmation level. */
void wldcore_list_handle_completed_propositions_input(wldcore_menu_stack_record_t* level) {
    s32 buttons;
    s32 depth;
    point32_t origin;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        wldcore_menu_dispatch_resume_handler();
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_list_completed_propositions_block_all_buttons(level);
        wldcore_menu_push_message_level(0x1061, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_list_completed_propositions_block_all_buttons(level);
        wldcore_proposition_prepare_report(
            level->proposition_list.entries[g_wldcore_window_panel_render_state.selected_index]);
        wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS);
        g_text_substitution_values[0] = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_HEAR_ABOUT_JOB, 0);
        origin.x = 0x48;
        origin.y = 0x20;
        wldcore_window_build_yes_no_panel(origin);
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS_BROWSE;
        g_wldcore_menu_stack_depth = depth + 1;
        g_wldcore_menu_stack_records_next[depth].proposition_report_confirm.argument
            = level->proposition_list.entries[g_wldcore_window_panel_render_state.selected_index];
    }
}
