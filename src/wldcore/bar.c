/* The bar, including the bar menu's suspend handler, rumor and proposition lists, rumor and proposition detail levels,
 * the accept confirmation, the number panel level and its dispatch-days step. */

#include "fft/bar.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/location.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/party.h"
#include "fft/proposition.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_bar_row_help_text_ids[];
extern s32* g_rumor_location_masks;

static void rumor_push_detail_level(s32 value);
static void rumor_open_list(void);

/* Gray out the bar menu while a sub-level opens over it: palette 10 on the
 * cursor, 6 on the side window and 2 on the frame, and restart the cursor's
 * press animation.
 *
 * Sibling of sound_novel_suspend_quit_level, differing only in the sequence
 * stored. The record stores may alias `level`, so they invalidate the cached
 * parameter loads and reproduce the target's reloads; `index` shares one
 * address computation between the last two. */
void bar_suspend_level(menu_stack_record_t* level) {
    s32 index;

    g_window_records[level->bar.cursor_window].palette = 10;
    g_window_records[level->bar.label_window].palette = 6;
    g_window_gs_image_records[level->bar.rows_image].palette = 2;
    g_window_records[level->bar.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
    index = level->bar.cursor_window;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
}

/* Leave the bar: pop its level, show the date and location label again, rebuild and push the
 * location menu for the current location, fade the map back in and clear the scratch buffer. */
static void bar_leave(bar_menu_level_t* unused_level) {
    s32 i;

    window_set_render_objects_visible(1);
    g_menu_stack_depth = g_menu_stack_depth - 1;
    g_system_flags &= ~(SYSTEM_FLAG_HIDE_CALENDAR_DATE | SYSTEM_FLAG_HIDE_LOCATION_LABEL);
    menu_stack_dispatch_suspend();
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = location_build_menu_entries(
        g_world_map_marker_state.marker.location_id, g_menu_stack_records[g_menu_stack_depth].panel.entries);
    location_push_menu_level(g_world_map_marker_state.marker.location_id);
    world_map_fade_start(WORLD_MAP_FADE_REDRAW_MAP, 0x10);
    world_map_clear_screen_and_set_clip_rect(0);
    /* Blank the backdrop image until the map is captured again. */
    for (i = WORLD_MAP_BACKDROP_WORDS - 1; i >= 0; i--) {
        g_world_map_scratch_buffer[i] = 0;
    }
}

/* Input step of the bar menu: Circle opens Rumors, Propositions, the job report or the in-progress
 * list (or leaves); Cross or Leave shows the goodbye line and, after a button, fades back to the
 * map; Select shows the row's help; Up and Down wrap. */
void bar_handle_input(menu_stack_record_t* level) {
    s32 flags;
    u32 buttons;

    if (g_window_gs_image_records[level->bar.rows_image].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    flags = g_system_flags;
    if (flags & SYSTEM_FLAG_FADING) {
        return;
    }

    if ((flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        bar_leave(&level->bar);
        return;
    }

    if (level->bar.mode != MENU_STACK_WINDOW_MODE_BROWSE) {
        if (!(g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->bar.mode = MENU_STACK_WINDOW_MODE_BROWSE;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        music_queue_start_track(MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP));
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        /* Same exit as the Leave row below; GCC cross-jumps the two copies. */
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BAR].selected_index = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_GOODBYE, 0);
        level->bar.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
        g_window_record_count -= 2;
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 3;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        bar_suspend_level(level);
        menu_stack_push_message_level(
            g_bar_row_help_text_ids[level->bar.entries[level->bar.selected_entry]] | 0x1000, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BAR].selected_index = level->bar.selected_entry;
        if (level->bar.entries[level->bar.selected_entry] != BAR_ROW_LEAVE) {
            g_window_records[level->bar.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
        }
        switch (level->bar.entries[level->bar.selected_entry]) {
        case BAR_ROW_RUMORS:
            message_block_load_if_changed(MESSAGE_BLOCK_RUMORS);
            bar_suspend_level(level);
            rumor_open_list();
            return;
        case BAR_ROW_AVAILABLE_PROPOSITIONS:
            message_block_load_if_changed(MESSAGE_BLOCK_PROPOSITIONS);
            bar_suspend_level(level);
            proposition_open_available_list();
            return;
        case BAR_ROW_RETURNED:
            bar_suspend_level(level);
            proposition_open_returned_list();
            return;
        case BAR_ROW_ACTIVE_PROPOSITIONS:
            bar_suspend_level(level);
            proposition_open_active_list();
            return;
        case BAR_ROW_LEAVE:
            g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BAR].selected_index = 0;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_GOODBYE, 0);
            level->bar.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
            g_window_record_count -= 2;
            g_window_render_record_count -= 1;
            g_window_render_object_count -= 3;
            return;
        }
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (level->bar.selected_entry != 0) {
            g_window_records[level->bar.cursor_window].position.y -= 0x10;
            level->bar.selected_entry--;
        } else {
            g_window_records[level->bar.cursor_window].position.y += (level->bar.entry_count - 1) * 0x10;
            level->bar.selected_entry = level->bar.entry_count - 1;
        }
    } else if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        if (level->bar.selected_entry < level->bar.entry_count - 1) {
            g_window_records[level->bar.cursor_window].position.y += 0x10;
            level->bar.selected_entry++;
        } else {
            g_window_records[level->bar.cursor_window].position.y -= (level->bar.entry_count - 1) * 0x10;
            level->bar.selected_entry = 0;
        }
    } else {
        return;
    }

    sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    if (level->bar.restore_welcome != 0) {
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_WELCOME, 0);
        level->bar.restore_welcome = 0;
    }
}

/* Open the bar's rumors list: every rumor whose location mask in
 * g_rumor_location_masks includes the current town and whose script
 * variable (EVENT_SCRIPT_VAR_RUMOR_FIRST + index) is set, listed by name
 * (TEXT_ID_RUMOR_NAME_BASE + index). The panel shows up to eight rows at
 * (0x4c, 0x50), 0x96 wide, and the MENU_STACK_LEVEL_RUMOR_LIST level is pushed.
 * With no rumors the bar menu's windows are reset instead and
 * TEXT_ID_NO_RUMORS is shown. */
static void rumor_open_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* colors;
    s32* locations;
    s32 mask;
    s32 entry;
    s32 count = 0;
    s32 i = 0;
    s32 record_index;

    colors = g_menu_stack_row_text_colors;
    values = g_menu_stack_entry_values;
    locations = g_rumor_location_masks;
    /* The masks hold one bit per town, location 0 in bit 14. */
    mask = 0x4000 >> g_world_map_marker_state.marker.location_id;
    for (; i < EVENT_SCRIPT_VAR_RUMOR_COUNT; i++, locations++) {
        /* A register operand keeps the loaded 32-bit value first in the `and`;
         * testing *locations directly lets expand_binop swap them. */
        entry = *locations;
        if ((entry & mask) && script_get_variable(i + EVENT_SCRIPT_VAR_RUMOR_FIRST) != 0) {
            *values = i + TEXT_ID_RUMOR_NAME_BASE;
            *colors = 0;
            colors++;
            values++;
            count++;
        }
    }

    if (count != 0) {
        g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_RUMORS_INTRO, 0);
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
        if (count < 8) {
            i = count;
        } else {
            i = 8;
        }
        layout.text_width = 0x96;
        origin.x = 0x4C;
        layout.visible_rows = i;
        origin.y = 0x50;
        menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_RUMORS, count, i);
        window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
            g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    } else {
        g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].palette
            = g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.label_window].palette
            = g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].palette = 0;
        g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].sequence
            = WINDOW_ANIM_CURSOR_IDLE;
        g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].frame_index
            = g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].anim_counter = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_NO_RUMORS, 0);
        g_menu_stack_records[g_menu_stack_depth - 1].bar.restore_welcome = 1;
        return;
    }

    g_menu_stack_ot_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    record_index = g_menu_stack_depth;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_RUMOR_LIST;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the rumors list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_RUMORS_INTRO again. */
void rumor_restore_list_prompt(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_RUMORS_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void rumor_block_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Pop the rumors list level: toggle the input-blocked system flag and
 * unblock the menu buttons. The caller hands in its menu level; the target
 * reads no argument. */
static void rumor_close_list(menu_stack_panel_level_t* level) {
    g_menu_stack_ot_offset = 1;
    g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
    --g_menu_stack_depth;
    g_menu_blocked_buttons = 0;
}

/* Input step of the bar's rumors list. Cross, or Circle on a non-empty list,
 * closes the panel thread and records the choice in result; Select on a
 * non-empty list opens help message 0x105e. Once the panel thread has ended,
 * the level closes and a chosen rumor's entry value (TEXT_ID_ENTRY_MASK of
 * g_menu_stack_entry_values) goes to rumor_push_detail_level;
 * otherwise the bar menu resumes.
 *
 * The target passes the level to rumor_close_list and
 * rumor_block_list_buttons even though neither reads the argument. */
void rumor_handle_list_input(menu_stack_record_t* level) {
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        rumor_close_list(&level->panel);
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_RUMORS);
            rumor_push_detail_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_RUMORS);
        g_system_flags &= ~SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        rumor_block_list_buttons(level);
        menu_stack_push_message_level(0x105E, 1);
        return;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Show rumor `value` in the scrollable text window, titled by the rumor-about message,
 * hide the bar menu's windows and push the rumor detail level, which keeps
 * their indices to show them again. coordinate_mode is always 0, so the
 * switch's mode-1 image row is never used. */
static void rumor_push_detail_level(s32 value) {
    s32 coordinate_mode = 0;
    s32 depth;
    u8 unused[8];

    g_text_substitution_values[0] = value + TEXT_ID_RUMOR_NAME_BASE;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_RUMOR_ABOUT, 0);
    g_menu_stack_records[g_menu_stack_depth].rumor_detail.rumor_index = value;
    g_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 1:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 0x70;
        break;
    case 0:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 0x60;
        break;
    }
    g_text_window.base.x = -122;
    g_text_window.base.y = -40;
    g_text_window.text_width = 0xDC;
    g_text_window.rows_per_page = 8;
    g_text_window.priority = 9;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[1] = -1;
    g_text_window.text_substitutions[0] = -1;
    text_window_open(&g_text_window);
    depth = g_menu_stack_depth;
    g_window_records[g_menu_stack_records[depth - 1].bar.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_records[depth].rumor_detail.first_window = g_menu_stack_records[depth - 1].bar.cursor_window;
    g_window_records[g_menu_stack_records[depth - 1].bar.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_records[depth].rumor_detail.second_window = g_menu_stack_records[depth - 1].bar.label_window;
    g_window_gs_image_records[g_menu_stack_records[depth - 1].bar.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
    g_menu_stack_records[depth].rumor_detail.render_index = g_menu_stack_records[depth - 1].bar.rows_image;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_RUMOR_DETAIL;
}

/* Close the rumor detail level once its text window is finished.
 *
 * Same wait as menu_stack_wait_text_then_pop; then shows the bar
 * menu's two windows and frame again, pops the stack and replays the frame's
 * pop-in while rumor_open_list rebuilds the list. */
void rumor_wait_detail_then_pop(menu_stack_record_t* level) {
    if (g_window_gs_image_records[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (text_window_step(&g_text_window) != 0) {
        return;
    }
    g_window_records[level->rumor_detail.first_window].flags ^= DISPLAY_OBJECT_HIDDEN;
    g_window_records[level->rumor_detail.second_window].flags ^= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[level->rumor_detail.render_index].flags ^= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth--;
    g_window_gs_image_records[level->rumor_detail.render_index].flags |= DISPLAY_OBJECT_POP_IN;
    rumor_open_list();
}

/* Open the bar's list of propositions on offer (the Propositions row of
 * bar_handle_input). If the bar menu is hidden (returning from a
 * proposition's levels) it is first shown again, its rows rebuilt with
 * bar_build_rows and grayed out.
 *
 * The list holds every proposition neither in progress nor completed, not
 * waiting out a failure (its g_proposition_states days are 0) and
 * offered in this town (proposition_is_available_at), by name
 * (TEXT_ID_PROPOSITION_NAME_BASE + id). With entries it shows
 * TEXT_ID_PROPOSITIONS_INTRO and up to six rows at (0x4c, 0x50), 0x96
 * wide, and pushes MENU_STACK_LEVEL_PROPOSITION_AVAILABLE_LIST; otherwise it
 * resumes the bar menu with TEXT_ID_NO_PROPOSITIONS, flagging its
 * content_render so the next cursor move restores the welcome line. */
void proposition_open_available_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 count;
    s32 i;
    s32 index;
    display_object_animated_t* record;
    s32 record_index;

    if (g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].flags
        & DISPLAY_OBJECT_HIDDEN) {
        g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.label_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        bar_build_rows(&g_menu_stack_records[g_menu_stack_depth - 1].bar);
        bar_suspend_level(&g_menu_stack_records[g_menu_stack_depth - 1]);
        index = g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window;
        record = &g_window_records[index];
        record->anim_counter = 0;
        record->frame_index = 0;
        g_window_records[index].sequence = WINDOW_ANIM_CURSOR_STILL;
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].flags
            |= DISPLAY_OBJECT_POP_IN;
    }

    count = 0;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (!(script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & (PROPOSITION_VARIABLE_IN_PROGRESS | PROPOSITION_VARIABLE_COMPLETED))
            && !(g_proposition_states[i] & PROPOSITION_STATE_DAYS_MASK)
            && proposition_is_available_at(i, g_world_map_marker_state.marker.location_id + 1) != 0) {
            g_menu_stack_entry_values[count] = i + TEXT_ID_PROPOSITION_NAME_BASE;
            g_menu_stack_row_text_colors[count] = 0;
            count++;
        }
    }

    if (count != 0) {
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_PROPOSITIONS_INTRO, 0);
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
        if (count < 6) {
            i = count;
        } else {
            i = 6;
        }
        layout.text_width = 0x96;
        origin.x = 0x4C;
        layout.visible_rows = i;
        origin.y = 0x50;
        menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_AVAILABLE_PROPOSITIONS, count, i);
        window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
            g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    } else {
        menu_stack_dispatch_resume();
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_NO_PROPOSITIONS, 0);
        g_menu_stack_records[g_menu_stack_depth - 1].bar.restore_welcome = 1;
        return;
    }

    record_index = g_menu_stack_depth;
    g_menu_stack_ot_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_records[record_index].panel.result = 0;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_PROPOSITION_AVAILABLE_LIST;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the available propositions list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_PROPOSITIONS_INTRO again. */
void proposition_restore_available_list_prompt(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_PROPOSITIONS_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void proposition_block_available_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Pop the available propositions list level: toggle the input-blocked system flag and
 * unblock the menu buttons. The caller hands in its menu level; the target
 * reads no argument. */
static void proposition_close_available_list(menu_stack_panel_level_t* level) {
    g_menu_stack_ot_offset = 1;
    g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
    --g_menu_stack_depth;
    g_menu_blocked_buttons = 0;
}

/* List level input step for the available propositions (cursor slot 3, Select
 * help 0x105f). Circle hands the chosen entry to
 * proposition_push_detail_level, but is refused with "I can't give
 * anymore jobs" once g_proposition_count has reached 8. For a
 * proposition already attempted (PROPOSITION_VARIABLE_ATTEMPTED) it
 * first hides the parent level's windows and render record, says "Think I
 * told you before but..." and waits in result 2 for Circle or Cross. */
void proposition_handle_available_list_input(menu_stack_record_t* level) {
    s32 depth;

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        proposition_close_available_list(&level->panel);
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_AVAILABLE_PROPOSITIONS);
            proposition_push_detail_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_AVAILABLE_PROPOSITIONS);
        menu_stack_dispatch_resume();
        return;
    }

    if (level->panel.result == 2) {
        if (g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            level->panel.result = 1;
            g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        }
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS)
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        proposition_block_available_list_buttons(level);
        menu_stack_push_message_level(0x105F, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        if (g_proposition_count >= 8) {
            sound_play_effect(SOUND_SFX_INVALID);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_NO_MORE_JOBS, 0);
            return;
        }
        sound_play_effect(SOUND_SFX_CONFIRM);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        if (script_get_variable(
                (g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK)
                + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
            & PROPOSITION_VARIABLE_ATTEMPTED) {
            depth = g_menu_stack_depth;
            g_window_records[g_menu_stack_records[depth - 2].bar.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_records[g_menu_stack_records[depth - 2].bar.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_gs_image_records[g_menu_stack_records[depth - 2].bar.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TOLD_BEFORE, 0);
            level->panel.result = 2;
            return;
        }
        level->panel.result = 1;
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Push the proposition detail level and show its description; the bar owner first gives his advice
 * on it with a 10% chance (20% after a failure, 40% after a bad failure), otherwise says 'This's
 * what I know about ...'. */
void proposition_push_detail_level(s32 value) {
    s32 chance;
    s32 index;
    s32 depth;
    u8 state = g_proposition_states[value];
    RECT bounds;
    point32_t* base;

    chance = (state & PROPOSITION_STATE_FAILURE) ? 0x14 : 0;
    if (state & PROPOSITION_STATE_BAD_FAILURE) {
        chance = 0x28;
    }
    if (!(state & (PROPOSITION_STATE_FAILURE | PROPOSITION_STATE_BAD_FAILURE))) {
        chance = 0xA;
    }
    if ((rand() * 100 >> 15) < chance) {
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, value + TEXT_ID_PROPOSITION_HINT_FIRST, 0);
    } else {
        g_text_substitution_values[0] = value + TEXT_ID_PROPOSITION_NAME_BASE;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_PROPOSITION_ABOUT, 0);
    }
    proposition_unpack_row(&g_proposition_selected_row, value);
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_detail.render_index = index;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.h = 0x80;
    bounds.x = 0;
    window_init_text_record(index, bounds, 0, value + TEXT_ID_WORLD_MESSAGE_BASE, g_window_image_buffer);
    base = &g_window_gs_image_records[index].base;
    g_window_gs_image_records[index].priority = 9;
    depth = g_menu_stack_depth;
    base->x = -0x7A;
    base->y = -0x28;
    g_window_records[g_menu_stack_records[depth - 1].bar.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[depth - 1].bar.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[depth - 1].bar.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_DETAIL;
}

/* Restore the proposition detail renderer's palette. */
void proposition_resume_detail_level(menu_stack_record_t* level) {
    g_window_gs_image_records[level->proposition_detail.render_index].palette = 0;
}

/* Select palette 2 for the window's render record. */
void proposition_suspend_detail_level(menu_stack_record_t* pair) {
    g_window_gs_image_records[pair->proposition_detail.render_index].palette = 2;
}

/* Input step of the proposition detail level. Cross closes it and reopens the
 * list (proposition_open_available_list). Circle stores the job's
 * total fee (fee_a + fee_b of the selected row) as the gil to pay, asks
 * TEXT_ID_JOB_FEE_PROMPT with the fee substituted (or
 * TEXT_ID_ACCEPT_JOB_PROMPT when it is free), opens the OK/NO panel
 * at (0x48, 0x20) and pushes MENU_STACK_LEVEL_PROPOSITION_ACCEPT. */
void proposition_handle_accept_input(menu_stack_record_t* owner) {
    s32 total;
    point32_t origin;

    if (g_window_gs_image_records[owner->proposition_detail.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_window_render_record_count--;
        g_window_render_object_count--;
        g_menu_stack_depth--;
        proposition_open_available_list();
    } else if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        total = g_proposition_selected_row.fee_a + g_proposition_selected_row.fee_b;
        g_proposition_gil_amount = total;
        if (total != 0) {
            g_text_substitution_values[0] = total;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_JOB_FEE_PROMPT, 0);
        } else {
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_ACCEPT_JOB_PROMPT, 0);
        }
        proposition_suspend_detail_level(owner);
        origin.x = 0x48;
        origin.y = 0x20;
        window_build_yes_no_panel(origin);
        g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_PROPOSITION_ACCEPT;
        g_menu_stack_depth++;
    }
}

/* Append an OK/NO choice panel at `origin` to the level being pushed: cursor and header windows
 * plus the OK/NO text record, with the cursor on OK. */
void window_build_yes_no_panel(point32_t origin) {
    RECT bounds;
    s32 dead_locals[10];
    s32 index;
    s32 depth;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].accept_confirm.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].accept_confirm.header_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CHOICE_PANEL_HEADER;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].accept_confirm.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record(index, bounds, 0, TEXT_ID_OK_NO, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 8;
    g_window_gs_image_records[index].base = origin;

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].accept_confirm.row = 0;
    g_menu_stack_records[depth].accept_confirm.mode = PROPOSITION_ACCEPT_MODE_CHOOSE;
    g_window_records[g_menu_stack_records[depth].accept_confirm.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 6;
    g_window_records[g_menu_stack_records[depth].accept_confirm.cursor_window].position.y
        = g_window_gs_image_records[index].base.y - -(g_menu_stack_records[depth].accept_confirm.row * 0x10 + 0xE);
    g_window_records[g_menu_stack_records[depth].accept_confirm.header_window].position.x
        = g_window_gs_image_records[index].base.x + 3;
    g_window_records[g_menu_stack_records[depth].accept_confirm.header_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
}

/* Input step of the proposition yes/no confirmation panel.
 *
 * Up/down toggle the highlighted row and move the cursor window by one
 * 16-pixel row. Cross and NO both tear the panel down and return through
 * proposition_open_available_list. OK first checks that at least one
 * generic, non-monster party member is free (the same filter as
 * proposition_load_send_unit_candidates) and that the war funds cover
 * g_proposition_gil_amount. With no unit it says
 * TEXT_ID_CHOOSE_MORE_UNITS, then TEXT_ID_CHOOSE_HUMAN_UNIT
 * after a button (NO_UNIT_MESSAGE mode); short of gil it says
 * TEXT_ID_NOT_ENOUGH_GIL; either way the next button closes the panel
 * (CLOSE_MESSAGE mode). A successful confirmation continues into
 * proposition_push_send_unit_level.
 *
 * The gotos are the target's shared tails: the mode 3 close, the "no"
 * dismissal and the no-unit failure jump into the cancel and gil-failure
 * blocks; duplicated tails do not cross-jump back into that shape. */
void proposition_handle_accept_confirm_input(menu_stack_record_t* level) {
    s32 available;
    s32 i;
    s32 row;
    party_unit_t* unit;

    if (g_window_gs_image_records[level->accept_confirm.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (level->accept_confirm.mode == PROPOSITION_ACCEPT_MODE_CLOSE_MESSAGE) {
        if (!(g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        goto close;
    }

    if (level->accept_confirm.mode == PROPOSITION_ACCEPT_MODE_NO_UNIT_MESSAGE) {
        if (g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_HUMAN_UNIT, 0);
            level->accept_confirm.mode = PROPOSITION_ACCEPT_MODE_CLOSE_MESSAGE;
        }
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
    dismiss:
        g_window_record_count -= 2;
        g_window_render_record_count--;
        g_window_render_object_count -= 3;
    close:
        g_window_render_record_count--;
        g_window_render_object_count--;
        g_menu_stack_depth -= 2;
        proposition_open_available_list();
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0 || world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        row = level->accept_confirm.row ^ 1;
        level->accept_confirm.row = row;
        g_window_records[level->accept_confirm.cursor_window].position.y
            = g_window_gs_image_records[level->accept_confirm.render_index].base.y - -(row * 0x10 + 0xE);
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (!(g_world_map_input_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }

    if (level->accept_confirm.row == 1) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        goto dismiss;
    }

    available = 0;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        unit = party_get_roster_unit(i);
        if (unit->proposition_status == 0 && unit->party_id != PARTY_ID_NONE
            && !(unit->gender_flags & UNIT_FLAG_MONSTER) && unit->sprite_set >= CHARACTER_IDENTITY_BOY) {
            available++;
        }
    }

    if (available == 0) {
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_MORE_UNITS, 0);
        level->accept_confirm.mode = PROPOSITION_ACCEPT_MODE_NO_UNIT_MESSAGE;
        goto fail;
    }

    if (script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) < g_proposition_gil_amount) {
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_NOT_ENOUGH_GIL, 0);
        level->accept_confirm.mode = PROPOSITION_ACCEPT_MODE_CLOSE_MESSAGE;
    fail:
        sound_play_effect(SOUND_SFX_INVALID);
        g_window_record_count -= 2;
        g_window_render_record_count--;
        g_window_render_object_count -= 3;
        return;
    }

    sound_play_effect(SOUND_SFX_CONFIRM);
    g_window_record_count -= 2;
    g_window_render_record_count--;
    g_window_render_object_count -= 3;
    g_window_render_record_count--;
    g_window_render_object_count--;
    g_menu_stack_depth -= 2;
    proposition_push_send_unit_level();
}

/* Pushes the "how many days" number panel of a proposition: shows
 * TEXT_ID_CHOOSE_DAYS with the row's minimum and maximum days,
 * starts the number panel thread on that range at the minimum, and withholds
 * Circle and Cross from the WORLD menu threads (the level's own step handles
 * them). */
void proposition_push_days_level(void) {
    point32_t origin;
    s32 record_index;
    s32 min_days = g_proposition_selected_row.min_days;
    s32 max_days = g_proposition_selected_row.max_days;

    g_text_substitution_values[0] = min_days;
    g_text_substitution_values[1] = max_days;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_DAYS, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_menu_stack_ot_offset = 8;
    origin.x = 0x38;
    origin.y = 0x20;
    g_window_panel_value = min_days;
    window_start_number_panel_thread(
        WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, min_days | (max_days << 16), TEXT_ID_DAYS_LABEL, &origin);
    record_index = g_menu_stack_depth;
    g_menu_stack_records[record_index].panel.result = 0;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_PROPOSITION_DAYS;
    g_menu_stack_depth = record_index + 1;
}

/* Input step of the days number panel, which runs on the panel thread. Cross
 * arms result -1 and Circle result 1, recording the entered days in
 * g_proposition_dispatch_days; either tells the thread to close. Once
 * it has, two levels are popped and result 1 goes on to the dispatch
 * confirmation, -1 back to the propositions list. */
void proposition_step_days_level(menu_stack_record_t* level) {
    if (level->panel.result != 0 && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_window_render_record_count--;
        g_window_render_object_count--;
        g_menu_stack_depth -= 2;
        if (level->panel.result == 1) {
            proposition_push_dispatch_confirm_level();
        } else {
            proposition_open_available_list();
        }
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_hud_value_mode = HUD_VALUE_FUNDS;
        level->panel.result = -1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        g_proposition_dispatch_days = g_window_panel_value;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    }
}
