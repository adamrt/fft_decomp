/* The bar, including the window publish slot, rumor and proposition lists, rumor and proposition detail levels, the
 * accept confirmation and the number panel level. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static void wldcore_bar_push_rumor_detail_level(s32 value);
static void wldcore_list_open_rumors(void);

/* Sibling of the resetter at 0x8007cd78, differing only in the value stored
 * into sequence. The record stores may alias `level`, so they invalidate
 * the cached parameter loads and reproduce the target's
 * reloads; `index` shares one address computation between the last two. */
void wldcore_window_publish_slot_b806(wldcore_menu_list_window_level_t* level) {
    s32 index;

    g_wldcore_window_records[level->main_window].palette = 10;
    g_wldcore_window_records[level->side_window].palette = 6;
    g_wldcore_window_render_records[level->frame_render].palette = 2;
    g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
    index = level->main_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}

static void wldcore_menu_pop_level_and_rebuild_screen(wldcore_menu_list_window_level_t* unused_level) {
    s32* selected = &g_wldcore_map_projection_state.marker.kind;
    s32 i;

    wldcore_window_set_render_objects_visible(1);
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
    g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER);
    wldcore_menu_dispatch_publish_handler();
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count
        = wldcore_map_build_location_menu_entries(
            *selected, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries);
    wldcore_menu_push_location_menu_level(*selected);
    wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_REDRAW_MAP, 0x10);
    wldcore_clear_screen_and_set_map_clip_rect(0);
    for (i = 0x77ff; i >= 0; i--) {
        g_wldcore_scratch_buffer[i] = 0;
    }
}

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
static void wldcore_list_open_rumors(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32* locations;
    s32 mask;
    s32 entry;
    s32 count = 0;
    s32 i = 0;
    s32 record_index;

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
static void wldcore_list_rumors_close_level(wldcore_menu_panel_level_t* level) {
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

static void wldcore_bar_push_rumor_detail_level(s32 value) {
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
static void wldcore_list_propositions_close_level(wldcore_menu_panel_level_t* level) {
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

void wldcore_proposition_push_detail_level(s32 value) {
    s32 chance;
    s32 index;
    s32 depth;
    u8 state = g_main_proposition_states[value];
    RECT bounds;
    point32_t* base;

    chance = (state & WLDCORE_PROPOSITION_STATE_FAILURE) ? 0x14 : 0;
    if (state & WLDCORE_PROPOSITION_STATE_BAD_FAILURE) {
        chance = 0x28;
    }
    if (!(state & (WLDCORE_PROPOSITION_STATE_FAILURE | WLDCORE_PROPOSITION_STATE_BAD_FAILURE))) {
        chance = 0xA;
    }
    if ((rand() * 100 >> 15) < chance) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, value + WLDCORE_TEXT_ID_PROPOSITION_HINT_FIRST, 0);
    } else {
        g_text_substitution_values[0] = value + TEXT_ID_PROPOSITION_NAME_BASE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_PROPOSITION_ABOUT, 0);
    }
    wldcore_unpack_proposition_row(g_wldcore_selected_proposition_row, value);
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_render.render_index = index;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.h = 0x80;
    bounds.x = 0;
    window_init_text_record_variant(
        index, bounds, 0, value + TEXT_ID_WORLD_MESSAGE_BASE, g_wldcore_window_image_buffer);
    base = &g_wldcore_window_render_records[index].base;
    g_wldcore_window_render_records[index].priority = 9;
    depth = g_wldcore_menu_stack_depth;
    base->x = -0x7A;
    base->y = -0x28;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_types_next[depth] = WLDCORE_MENU_LEVEL_PROPOSITION_DETAIL;
}

/* Restore the proposition detail renderer's palette. */
void wldcore_window_restore_proposition_detail_palette(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_render_records[level->window_render.render_index].palette = 0;
}

/* Select palette 2 for the window's render record. */
void wldcore_window_set_render_palette_2(wldcore_window_render_pair_t* pair) {
    g_wldcore_window_render_records[pair->render_index].palette = 2;
}

/* Menu input step: cancel (0x40) closes this level and returns through
 * 0x80072a18; confirm (0x20) stores the 0x8009f2f4 + 0x8009f2f6 total, shows
 * message 0xb810 (with the total substituted) or 0xb80f when it is zero,
 * opens the yes/no panel at (0x48, 0x20), and pushes a level tagged 0xa. */
void wldcore_proposition_handle_accept_input(wldcore_window_render_pair_t* owner) {
    s32 total;
    point32_t origin;

    if (g_wldcore_window_render_records[owner->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_available_propositions();
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        total = g_wldcore_selected_proposition_row[0].fee_a + g_wldcore_selected_proposition_row[0].fee_b;
        g_wldcore_proposition_gil_amount = total;
        if (total != 0) {
            g_text_substitution_values[0] = total;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOB_FEE_PROMPT, 0);
        } else {
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_ACCEPT_JOB_PROMPT, 0);
        }
        wldcore_window_set_render_palette_2(owner);
        origin.x = 0x48;
        origin.y = 0x20;
        wldcore_window_build_yes_no_panel(origin);
        g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_ACCEPT;
        g_wldcore_menu_stack_depth++;
    }
}

void wldcore_window_build_yes_no_panel(point32_t origin) {
    RECT bounds;
    s32 dead_locals[10];
    s32 index;
    s32 depth;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.header_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record_variant(index, bounds, 0, WLDCORE_TEXT_ID_OK_NO, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].base = origin;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].choice_panel.row = 0;
    g_wldcore_menu_stack_records_next[depth].choice_panel.mode = WLDCORE_ACCEPT_CONFIRM_MODE_CHOOSE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y
        - -(g_wldcore_menu_stack_records_next[depth].choice_panel.row * 0x10 + 0xE);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
}

/* Input step of the proposition yes/no confirmation panel.
 *
 * Up/down toggle the highlighted row and move the cursor window by one
 * 16-pixel row. Cancel (0x40) and "no" both tear the panel down and return
 * through wldcore_list_open_available_propositions. "Yes" first checks that at least one non-guest,
 * non-monster, generic party unit is free (the same filter as
 * wldcore_proposition_load_send_unit_candidates) and that script variable 0x2c
 * is at least g_wldcore_proposition_gil_amount; failing either shows message 0xb822 or 0xb811 and
 * parks the level in mode 1 or 3 waiting for a button, and mode 1 then chains
 * message 0xb8e0 before mode 3 closes. A successful confirmation continues
 * into wldcore_proposition_push_send_unit_level.
 *
 * The gotos are the target's shared tails: the mode 3 close, the "no"
 * dismissal and the no-unit failure jump into the cancel and gil-failure
 * blocks; duplicated tails do not cross-jump back into that shape. */
void wldcore_proposition_handle_accept_confirm_input(wldcore_menu_choice_panel_level_t* level) {
    s32 available;
    s32 i;
    s32 row;
    party_unit_t* unit;

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (level->mode == WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE) {
        if (!(g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        goto close;
    }
    if (level->mode == WLDCORE_ACCEPT_CONFIRM_MODE_NO_UNIT_MESSAGE) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_HUMAN_UNIT, 0);
            level->mode = WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE;
        }
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
    dismiss:
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 3;
    close:
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_menu_stack_depth -= 2;
        wldcore_list_open_available_propositions();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        row = level->row ^ 1;
        level->row = row;
        g_wldcore_window_records[level->cursor_window].position.y
            = g_wldcore_window_render_records[level->render_index].base.y - -(row * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (!(g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }
    if (level->row == 1) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        goto dismiss;
    }
    available = 0;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        unit = wldcore_get_party_unit(i);
        if (unit->proposition_status == 0 && unit->party_id != PARTY_ID_NONE
            && !(unit->gender_flags & UNIT_FLAG_MONSTER) && unit->sprite_set >= CHARACTER_IDENTITY_BOY) {
            available++;
        }
    }
    if (available == 0) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_MORE_UNITS, 0);
        level->mode = WLDCORE_ACCEPT_CONFIRM_MODE_NO_UNIT_MESSAGE;
        goto fail;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) < g_wldcore_proposition_gil_amount) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_NOT_ENOUGH_GIL, 0);
        level->mode = WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE;
    fail:
        wldcore_sound_play_effect(MAIN_SFX_INVALID);
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 3;
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    g_wldcore_window_record_count -= 2;
    g_wldcore_window_render_record_count--;
    g_wldcore_window_render_object_count -= 3;
    g_wldcore_window_render_record_count--;
    g_wldcore_window_render_object_count--;
    g_wldcore_menu_stack_depth -= 2;
    wldcore_proposition_push_send_unit_level();
}

/* Pushes the "how many days" number panel of a proposition: shows prompt
 * 0xB813 with the row's minimum and maximum days, starts the number panel
 * thread on that range starting at the minimum, and withholds Circle and
 * Cross from the WORLD menu threads (the level's own step handles them). */
void wldcore_menu_push_number_panel_level(void) {
    point32_t origin;
    s32 record_index;
    s32 x = g_wldcore_selected_proposition_row[0].min_days;
    s32 y = g_wldcore_selected_proposition_row[0].max_days;

    g_text_substitution_values[0] = x;
    g_text_substitution_values[1] = y;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_DAYS, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_wldcore_menu_ordering_table_offset = 8;
    origin.x = 0x38;
    origin.y = 0x20;
    g_wldcore_active_menu_value = x;
    wldcore_window_init_number_panel_render_thread(
        WLDCORE_THREAD_SLOT_WINDOW_PANEL, x | (y << 16), WLDCORE_TEXT_ID_DAYS_LABEL, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_NUMBER_PANEL;
    g_wldcore_menu_stack_depth = record_index + 1;
}
