#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Input step for the world-map town menu list level.
 *
 * Waits out the screen transition (system flags 4/8), then: cancel or the
 * "leave" row (entry 4) starts the fade back to the map and announces text
 * 0xb807; Select opens the help message for the highlighted row from
 * g_wldcore_bar_row_help_text_ids; confirm dispatches on the row value through a five-entry jump
 * table at 0x80067034 - 0 rumors, 1 the proposition-location list
 * (wldcore_list_open_available_propositions), 2 wldcore_list_open_completed_propositions, 3 the proposition list, 4
 * leave; and up/down wrap the cursor, moving the cursor window 16 pixels a row; a move re-announces text 0xb806 when
 * content_render is set, and clears it.
 */

void wldcore_bar_handle_menu_input(wldcore_menu_list_window_level_t* level) {
    s32 flags;
    u32 buttons;
    s32* selected_entry;

    if (g_wldcore_window_render_records[level->frame_render].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    flags = g_main_system_flags;
    if (flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }
    if ((flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_menu_pop_level_and_rebuild_screen(level);
        return;
    }

    if (level->mode != WLDCORE_LIST_WINDOW_MODE_BROWSE) {
        if (!(g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Same exit as case 4 below; GCC cross-jumps the two copies. */
        g_wldcore_bar_menu_saved_cursor = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_GOODBYE, 0);
        level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 3;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_publish_slot_b806(level);
        wldcore_menu_push_message_level(
            g_wldcore_bar_row_help_text_ids[level->entries[level->selected_entry]] | 0x1000, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        selected_entry = &level->selected_entry;
        g_wldcore_bar_menu_saved_cursor = *selected_entry;
        if (level->entries[*selected_entry] != WLDCORE_BAR_MENU_ROW_LEAVE) {
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        }
        switch (level->entries[level->selected_entry]) {
        case WLDCORE_BAR_MENU_ROW_RUMORS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_RUMORS);
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_rumors();
            return;
        case WLDCORE_BAR_MENU_ROW_AVAILABLE_PROPOSITIONS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_PROPOSITIONS);
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_available_propositions();
            return;
        case WLDCORE_BAR_MENU_ROW_COMPLETED_PROPOSITIONS:
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_completed_propositions();
            return;
        case WLDCORE_BAR_MENU_ROW_PROPOSITIONS:
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_propositions();
            return;
        case WLDCORE_BAR_MENU_ROW_LEAVE:
            g_wldcore_bar_menu_saved_cursor = 0;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_GOODBYE, 0);
            level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count -= 1;
            g_wldcore_window_render_object_count -= 3;
            return;
        }
        return;
    }

    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->selected_entry != 0) {
            g_wldcore_window_records[level->main_window].position.y -= 0x10;
            level->selected_entry--;
        } else {
            g_wldcore_window_records[level->main_window].position.y += (level->entry_count - 1) * 0x10;
            level->selected_entry = level->entry_count - 1;
        }
    } else if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].position.y += 0x10;
            level->selected_entry++;
        } else {
            g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * 0x10;
            level->selected_entry = 0;
        }
    } else {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    if (level->content_render != 0) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_WELCOME, 0);
        level->content_render = 0;
    }
}

/* Opens a panel list of the 64 entries whose location mask (32-bit table at
 * g_wldcore_rumor_location_masks) includes the current location and whose script variable
 * 0x2d1 + index is set (entry values 0xc800 + index); shows up to eight rows
 * at (0x4c, 0x50) with width 0x96 and appends its record tagged 6. With no
 * entries it resets the parent window-pair level's records and shows text
 * 0xb80a instead. */
void wldcore_list_open_rumors(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32* locations;
    s32 mask;
    s32 entry;
    s32 count;
    s32 i;
    s32 record_index;

    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    locations = g_wldcore_rumor_location_masks;
    mask = 0x4000 >> g_wldcore_map_projection_state.marker.kind;
    for (; i < EVENT_SCRIPT_VAR_RUMOR_COUNT; i++, locations++) {
        /* A register operand keeps the loaded 32-bit value first in the `and`;
         * testing *locations directly lets expand_binop swap them. */
        entry = *locations;
        if ((entry & mask) && script_get_variable(i + EVENT_SCRIPT_VAR_RUMOR_FIRST) != 0) {
            *values = i + (s16)TEXT_ID_RUMOR_NAME_BASE;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    if (count != 0) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RUMORS_INTRO, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
        if (count < 8) {
            i = count;
        } else {
            i = 8;
        }
        layout.text_width = 0x96;
        origin.x = 0x4C;
        layout.visible_rows = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_RUMORS, count, i);
        wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    } else {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair_render.first_window]
            .palette = g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                    .window_pair_render.second_window]
                           .palette
            = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                  .window_pair_render.render_index]
                  .palette = 0;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair_render.first_window]
            .sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair_render.first_window]
            .frame_index = g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                        .window_pair_render.first_window]
                               .anim_counter = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_NO_RUMORS, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_pair_render.panel_rows = 1;
        return;
    }
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_RUMORS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Resume handler of the rumors list: block Circle, Cross and Select for the
 * levels underneath and show prompt 0xB808 again. */
void wldcore_list_rumors_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RUMORS_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level runs. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_rumors_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Pop the rumors list level: toggle the input-blocked system flag and
 * unblock the menu buttons. */
/* Target 0x80072404. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_rumors_close_level(wldcore_menu_panel_level_t* level) {
    g_wldcore_menu_ordering_table_offset = 1;
    g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    --g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = 0;
}

/* Yes/no list-level input step. Cancel (0x40) or confirm (0x20) starts
 * thread 12's closing animation and records the choice in result; 0x100
 * with a non-empty list opens help message 0x105e. Once thread 12 finishes,
 * the level closes and a confirmed choice passes the selected entry's value
 * (low 11 bits of g_wldcore_list_entry_values) to 0x80072618, otherwise the bar menu resumes.
 *
 * The target passes the level to 0x80072404 and 0x800723f0 even though
 * neither reads an argument; their local prototypes here take it. */
void wldcore_list_handle_rumors_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        wldcore_list_rumors_close_level(level);
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_RUMORS);
            wldcore_bar_push_rumor_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_RUMORS);
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_rumors_block_all_buttons(level);
        wldcore_menu_push_message_level(0x105E, 1);
        return;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}
