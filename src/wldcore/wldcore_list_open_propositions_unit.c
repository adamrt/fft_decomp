#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Opens the active-proposition panel list as menu level type 0x12.
 *
 * Complete propositions are skipped. Each surviving proposition contributes
 * its display value, row flags, and source-record index to the new menu level.
 * Before pushing the level, this hides the previous window pair when visible.
 * The otherwise unused point record preserves the target's 24-byte local area;
 * GCC removes its operations but retains its stack slot. */
void wldcore_list_open_propositions(void) {
    point32_t origin;
    wldcore_window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 loaded_count;
    s32 previous;
    s32 total;
    s32 entry_value_base;
    s16* row_flags;
    s32 active_row_flag;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_IN_PROGRESS, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        row_flags = g_wldcore_list_row_flags;
        active_row_flag = 8;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED) == 0) {
                g_wldcore_list_entry_values[count] = g_main_active_propositions[i].proposition_id + entry_value_base;
                if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RECALLED) != 0) {
                    row_flags[count] = active_row_flag;
                } else {
                    row_flags[count] = 0;
                }
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }
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
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS, g_main_save_proposition_count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    previous = g_wldcore_menu_stack_depth - 1;
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    if ((g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
                .flags
            & WORLD_RENDER_RECORD_HIDDEN)
        != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.first_window = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITIONS;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Resume handler of the active propositions list: block Circle, Cross and
 * Select for the levels underneath and show prompt 0xB81B again. */
void wldcore_bar_active_propositions_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_IN_PROGRESS, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level runs. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_bar_active_propositions_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* List-menu input step: cancel (0x40) and confirm (0x20) close the level
 * through WORLD thread 12, and 0x100 opens help text 0x1062. Once the thread
 * finishes, a confirmed level dispatches the selected row's value through
 * 0x8007af9c.
 *
 * The target keeps each `level->panel.result` store ahead of the `li $a0,5`
 * argument of the following cursor-state call, which lands in the jal delay
 * slot; without the empty barriers GCC schedules the li first.
 */
void wldcore_bar_handle_active_propositions_input(wldcore_menu_stack_record_t* level) {
    u32 buttons;
    s32 unused[2]; /* 8-byte frame slot the target reserves */

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->panel.result != 0) {
            wldcore_proposition_push_cancel_confirm_level(
                level->panel.entries[g_wldcore_window_panel_render_state.selected_index]);
        } else {
            wldcore_menu_dispatch_resume_handler();
        }
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->panel.result = 0;
        /* Scheduling barrier: keeps the store ahead of the cursor argument. */
        __asm__("");
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if (buttons & PSX_PAD_SELECT) {
        wldcore_bar_active_propositions_block_all_buttons(&level->panel);
        wldcore_menu_push_message_level(0x1062, 1);
    } else if (buttons & PSX_PAD_CIRCLE) {
        if (g_wldcore_list_row_flags[g_wldcore_window_panel_render_state.selected_index] != 0) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RETURN_TOMORROW, 0);
        } else {
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            level->panel.result = 1;
            /* Scheduling barrier: keeps the store ahead of the cursor argument. */
            __asm__("");
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS);
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        }
    }
}
