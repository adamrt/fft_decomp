/* Tutorial category list step, the formation tutorial list level and the mask-1 entry list open. */

#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Per-frame step of the tutorial menu list level.
 *
 * Mode 0 runs the cursor and the four entries: formation tutorials, the
 * wldcore_list_open_tutorial_mask_1_entries list, the chapter-5 tutorial topics, and entry 3 (or cancel),
 * which starts WORLD thread 14 on text 0xb8de and moves to mode 1. Mode 1
 * waits for confirm/cancel and starts the fade out, mode 2 clears VRAM once
 * the fade flag has cleared and mode 3 pops the level: with system flag 0x80000 it
 * hands back to the caller through 0x10000000, otherwise it fades in and calls
 * wldcore_menu_push_entry_window_level. */
void wldcore_list_handle_tutorial_categories_input(wldcore_menu_list_window_level_t* level) {
    u32 buttons;
    s32* selected_entry;

    if (g_wldcore_window_render_records[level->frame_render].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) {
        if (level->mode == WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP && !(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT;
            wldcore_gfx_clear_vram_and_scratch(1);
        }
        if (level->mode == WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            if ((g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_TUTORIAL) == 0 || g_wldcore_audio_queue.count == 0) {
                g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                wldcore_window_set_render_objects_visible(1);
                g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
                g_main_system_flags = g_main_system_flags
                    & ~(MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
                        | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER);
                wldcore_menu_dispatch_publish_handler();
                if (g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_TUTORIAL) {
                    g_main_system_flags
                        = (g_main_system_flags ^ (MAIN_SYSTEM_FLAG_RUNNING | MAIN_SYSTEM_FLAG_TITLE_TUTORIAL))
                        | MAIN_SYSTEM_FLAG_RETURN_TO_TITLE;
                    return;
                }
                wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_REDRAW_MAP, 0x10);
                wldcore_menu_push_entry_window_level();
            }
        }
        return;
    }
    if (level->mode == WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            level->mode = WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
            g_main_system_flags = g_main_system_flags | MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
            if (g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_TUTORIAL) {
                wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
                wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 0);
            } else {
                wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
            }
            g_wldcore_tutorial_category_saved_cursor = 0;
        }
        return;
    }
    if (g_wldcore_audio_queue.count != 0) {
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_PROMPT, 0);
        level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 3;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_publish_slot_b8dd(level);
        wldcore_menu_push_message_level(level->entries[level->selected_entry] + 0x108B, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        selected_entry = &level->selected_entry;
        g_wldcore_tutorial_category_saved_cursor = *selected_entry;
        switch (level->entries[*selected_entry]) {
        case WLDCORE_TUTORIAL_CATEGORY_ROW_FORMATION:
            wldcore_window_publish_slot_b8dd(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_formation_tutorials();
            return;
        case WLDCORE_TUTORIAL_CATEGORY_ROW_MASK_1_ENTRIES:
            wldcore_window_publish_slot_b8dd(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_tutorial_mask_1_entries();
            return;
        case WLDCORE_TUTORIAL_CATEGORY_ROW_TOPICS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_TUTORIAL_TOPICS);
            wldcore_window_publish_slot_b8dd(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_tutorial_topics();
            return;
        case WLDCORE_TUTORIAL_CATEGORY_ROW_LEAVE:
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_PROMPT, 0);
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
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].position.y += 0x10;
            level->selected_entry++;
        } else {
            g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * 0x10;
            level->selected_entry = 0;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
}

/* Opens a seven-row panel list (entry values 0xc000 + row) whose rows are
 * flagged 8 when the matching bit of 0x8004c6bc is set, shown at
 * (0x54, 0x50) with width 0x8c; appends its record tagged 0x2c.
 *
 * The flag value lives in its own variable so the target keeps 8 in a saved
 * register across the loop. */
void wldcore_list_open_formation_tutorials(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 i;
    s32 count;
    s32 record_index;
    s32 enabled;

    i = 0;
    enabled = 8;
    for (; i < 7; i++) {
        g_wldcore_list_entry_values[i] = i + (s16)TEXT_ID_TUTORIAL_NAME_BASE;
        if (wldcore_test_bit(g_main_tutorial_progress_masks, i) != 0) {
            g_wldcore_list_row_flags[i] = enabled;
        } else {
            g_wldcore_list_row_flags[i] = 0;
        }
    }
    count = i;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = i;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x54;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

void wldcore_list_formation_tutorials_restore_blocked_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_formation_tutorials_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* List level input step for cursor slot 18 (help message 0x108b). A confirmed
 * entry starts fade 2 and thread 12's closing animation; once it finishes the
 * level switches to state 2, sets system flag 0x40 and starts thread 14. When
 * thread 14 ends, the selected index is marked in the bitset at 0x8004c6bc,
 * menu brightness is restored and the formation screen opens for it. */
void wldcore_list_handle_formation_tutorials_input(wldcore_menu_panel_level_t* level) {
    if (level->result == 2) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            s16* selected = &g_wldcore_window_panel_render_state.selected_index;

            wldcore_set_bit_value(g_main_tutorial_progress_masks, *selected, 1);
            world_menu_set_brightness(0x80, 0x80, 0x80);
            world_formation_run_screen_for_menu_index(*selected);
            g_wldcore_menu_ordering_table_offset = 1;
            g_menu_blocked_buttons = 0;
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED | MAIN_SYSTEM_FLAG_DISPLAY_OFF;
            g_wldcore_menu_stack_depth--;
            wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS, 0, 0);
        }
        return;
    }
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS);
            level->result = 2;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_DISPLAY_OFF;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
            return;
        }
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_formation_tutorials_block_all_buttons(level);
        wldcore_menu_push_message_level(0x108B, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
    }
}

/* Opens an eight-row panel list (entry values 0xc010 + row) whose rows are
 * flagged 8 when bit 32 + row of 0x8004c6bc is set, shown at (0x54, 0x50)
 * with width 0x8c; appends its record tagged 0x2d. Sibling of
 * wldcore_list_open_formation_tutorials. */
void wldcore_list_open_tutorial_mask_1_entries(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 i;
    s32 count;
    s32 record_index;

    for (i = 0; i < 8; i++) {
        g_wldcore_list_entry_values[i] = i - 0x3FF0;
        if (wldcore_test_bit(&g_main_tutorial_progress_masks[1], i) != 0) {
            g_wldcore_list_row_flags[i] = 8;
        } else {
            g_wldcore_list_row_flags[i] = 0;
        }
    }
    count = i;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = i;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x54;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES;
    g_wldcore_menu_stack_depth = record_index + 1;
}
