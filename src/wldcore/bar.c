/* The bar: it offers rumors and propositions, preserving its menu windows while the player opens
 * details or accepts work, then dispatches an accepted proposition by choosing participants and
 * confirming their assignment; day counters track their return. */

#include "fft/world/bar.h"
#include "fft/game/battle_unit.h"
#include "fft/game/character_identity.h"
#include "fft/game/party.h"
#include "fft/game/party_unit.h"
#include "fft/gfx/display_object.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/portrait.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_screen.h"
#include "fft/menu/menu_selection.h"
#include "fft/party/unit_panel.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/input.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "fft/world/location.h"
#include "fft/world/menu_stack.h"
#include "fft/world/menu_stack_records.h"
#include "fft/world/proposition.h"
#include "fft/world/window.h"
#include "fft/world/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void open_rumor_list(void);
static void open_rumor_detail(s32 value);
static void open_available_list(void);
static void open_proposition_detail(s32 value);
static void open_dispatch_confirm(void);
static s32 count_returned_propositions(s32 location);
static void open_participants(void);
static s32 build_candidates(proposition_send_unit_level_t* level);

/* rumors and propositions */

extern u8 g_bar_row_help_text_ids[];
extern s32* g_rumor_location_masks;

/* Opens the bar (MENU_STACK_LEVEL_BAR): hides the map, loads the bar
 * background (set 0x16 or 0x17 by SCRIPT_VAR_TOWN_BACKGROUND, else 0),
 * starts the message box on TEXT_ID_BAR_WELCOME, and allocates two
 * window records and one render record for the level before
 * bar_build_rows fills it. */
void bar_open_menu(void) {
    s32 value;
    s32 set;
    s32 record_index;

    world_map_clear_screen(1);
    display_object_set_queues_enabled(0);
    world_map_start_fade(0, 0x10);
    value = script_variable_get(SCRIPT_VAR_TOWN_BACKGROUND);
    set = value == 1 ? 0x16 : 0;
    if (value == 2) {
        set = 0x17;
    }
    world_map_load_background_picture_set(set);
    file_wait_pending();
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CALENDAR_DATE | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_WELCOME, 0);
    g_menu_stack_records[g_menu_stack_depth].bar.cursor_window
        = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].bar.label_window
        = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].bar.rows_image
        = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    bar_build_rows(&g_menu_stack_records[g_menu_stack_depth].bar);
    record_index = g_menu_stack_depth;
    g_menu_stack_records[record_index].bar.restore_welcome = 0;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BAR;
    g_menu_stack_depth = record_index + 1;
}

/* Fill the bar menu: Rumors always, Propositions once SCRIPT_VAR_PROPOSITIONS_ENABLED is
 * set, the job report while jobs have returned to this town, the in-progress list while some
 * accepted job has not returned, then Leave; draw the rows into the frame window. */
void bar_build_rows(bar_menu_level_t* level) {
    s32 index;
    point32_t point;
    RECT bounds;
    s32 unused[8];
    menu_cursor_state_t* cursor;
    point32_t* base;
    display_object_gs_image_t* render;
    s32 i;

    index = level->cursor_window;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 0xA;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    index = level->label_window;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_LIST_SIDE_LABEL;
    g_display_object_animated_pool[index].priority = 0xA;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    level->entries[0] = BAR_ROW_RUMORS;
    level->entry_count = 1;
    if (script_variable_get(SCRIPT_VAR_PROPOSITIONS_ENABLED) != 0) {
        level->entries[1] = BAR_ROW_AVAILABLE_PROPOSITIONS;
        level->entry_count++;
    }

    if (count_returned_propositions(g_world_map_marker_state.marker.location_id) != 0) {
        level->entries[level->entry_count] = BAR_ROW_RETURNED;
        level->entry_count++;
    }

    if (g_proposition_count != count_returned_propositions(-1)) {
        level->entries[level->entry_count] = BAR_ROW_ACTIVE_PROPOSITIONS;
        level->entry_count++;
    }

    level->entries[level->entry_count] = BAR_ROW_LEAVE;
    level->entry_count++;
    index = level->rows_image;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 72;
    bounds.h = level->entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 0xA;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_display_object_gs_image_pool[index].position;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->entry_count; i++) {
        window_render_text_entry(index, level->entries[i] + TEXT_ID_WORLD_MENU_BASE, point, g_window_image_buffer);
        point.y += 0x10;
    }

    window_load_render_record_image(index, g_window_image_buffer);
    cursor = &g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BAR];
    if (level->entry_count < cursor->selected_index + 1) {
        cursor->selected_index = 0;
    }

    g_menu_stack_records[g_menu_stack_depth].bar.selected_entry = cursor->selected_index;
    g_menu_stack_records[g_menu_stack_depth].bar.mode = MENU_STACK_WINDOW_MODE_BROWSE;
    render = &g_display_object_gs_image_pool[index];
    g_display_object_animated_pool[level->cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 6;
    g_display_object_animated_pool[level->cursor_window].position.y
        = g_display_object_gs_image_pool[index].position.y + cursor->selected_index * 0x10 + 0xE;
    g_display_object_animated_pool[level->label_window].position.x = render->position.x + 3;
    g_display_object_animated_pool[level->label_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
}

/* Restore the bar list's palettes and cursor animation, then show its welcome text. */
void bar_resume_menu(menu_stack_record_t* level) {
    s32 main_window;
    s32 side_window;
    s32 slot;
    /* The original frame reserves eight unused bytes. */
    s32 pad[2];

    main_window = level->bar.cursor_window;
    side_window = level->bar.label_window;
    g_display_object_gs_image_pool[level->bar.rows_image].palette = 0;
    g_display_object_animated_pool[side_window].palette = 0;
    g_display_object_animated_pool[main_window].palette = 0;
    slot = level->bar.cursor_window;
    g_display_object_animated_pool[slot].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    slot = level->bar.cursor_window;
    g_display_object_animated_pool[slot].anim_counter = 0;
    g_display_object_animated_pool[slot].frame_index = 0;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_WELCOME, 0);
}

/* Gray out the bar menu while a sub-level opens over it: palette 10 on the
 * cursor, 6 on the side window and 2 on the frame, and restart the cursor's
 * press animation.
 *
 * Sibling of novel_suspend_quit_menu, differing only in the sequence
 * stored. The record stores may alias `level`, so they invalidate the cached
 * parameter loads and reproduce the target's reloads; `index` shares one
 * address computation between the last two. */
void bar_suspend_menu(menu_stack_record_t* level) {
    s32 index;

    g_display_object_animated_pool[level->bar.cursor_window].palette = 10;
    g_display_object_animated_pool[level->bar.label_window].palette = 6;
    g_display_object_gs_image_pool[level->bar.rows_image].palette = 2;
    g_display_object_animated_pool[level->bar.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
    index = level->bar.cursor_window;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
}

/* Leave the bar: pop its level, show the date and location label again, rebuild and push the
 * location menu for the current location, fade the map back in and clear the scratch buffer. */
static void leave_menu(bar_menu_level_t* unused_level) {
    s32 i;

    display_object_set_queues_enabled(1);
    g_menu_stack_depth--;
    g_world_map_flags &= ~(WORLD_MAP_FLAG_HIDE_CALENDAR_DATE | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL);
    menu_stack_dispatch_suspend();
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = location_build_menu_entries(
        g_world_map_marker_state.marker.location_id, g_menu_stack_records[g_menu_stack_depth].panel.entries);
    location_open_menu(g_world_map_marker_state.marker.location_id);
    world_map_start_fade(WORLD_MAP_FADE_REDRAW_MAP, 0x10);
    world_map_clear_screen(0);
    /* Blank the backdrop image until the map is captured again. */
    for (i = WORLD_MAP_BACKDROP_WORDS - 1; i >= 0; i--) {
        g_world_map_scratch_buffer[i] = 0;
    }
}

/* Input step of the bar menu: Circle opens Rumors, Propositions, the job report or the in-progress
 * list (or leaves); Cross or Leave shows the goodbye line and, after a button, fades back to the
 * map; Select shows the row's help; Up and Down wrap. */
void bar_update_menu(menu_stack_record_t* level) {
    s32 flags;
    u32 buttons;

    if (g_display_object_gs_image_pool[level->bar.rows_image].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    flags = g_world_map_flags;
    if (flags & WORLD_MAP_FLAG_FADING) {
        return;
    }

    if ((flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE)) {
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        leave_menu(&level->bar);
        return;
    }

    if (level->bar.mode != MENU_STACK_WINDOW_MODE_BROWSE) {
        if (!(g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->bar.mode = MENU_STACK_WINDOW_MODE_BROWSE;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        world_map_start_fade(WORLD_MAP_FADE_OUT, 0x10);
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
        g_display_object_animated_count -= 2;
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 3;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        bar_suspend_menu(level);
        menu_stack_open_message(g_bar_row_help_text_ids[level->bar.entries[level->bar.selected_entry]] | 0x1000, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BAR].selected_index = level->bar.selected_entry;
        if (level->bar.entries[level->bar.selected_entry] != BAR_ROW_LEAVE) {
            g_display_object_animated_pool[level->bar.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
        }
        switch (level->bar.entries[level->bar.selected_entry]) {
        case BAR_ROW_RUMORS:
            message_load_block_if_changed(MESSAGE_BLOCK_RUMORS);
            bar_suspend_menu(level);
            open_rumor_list();
            return;
        case BAR_ROW_AVAILABLE_PROPOSITIONS:
            message_load_block_if_changed(MESSAGE_BLOCK_PROPOSITIONS);
            bar_suspend_menu(level);
            open_available_list();
            return;
        case BAR_ROW_RETURNED:
            bar_suspend_menu(level);
            proposition_open_returned_list();
            return;
        case BAR_ROW_ACTIVE_PROPOSITIONS:
            bar_suspend_menu(level);
            proposition_open_active_list();
            return;
        case BAR_ROW_LEAVE:
            g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BAR].selected_index = 0;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_GOODBYE, 0);
            level->bar.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
            g_display_object_animated_count -= 2;
            g_display_object_image_count -= 1;
            g_display_object_queue_count -= 3;
            return;
        }
        return;
    }

    if (input_update_repeat(PSX_PAD_UP)) {
        if (level->bar.selected_entry != 0) {
            g_display_object_animated_pool[level->bar.cursor_window].position.y -= 0x10;
            level->bar.selected_entry--;
        } else {
            g_display_object_animated_pool[level->bar.cursor_window].position.y += (level->bar.entry_count - 1) * 0x10;
            level->bar.selected_entry = level->bar.entry_count - 1;
        }
    } else if (input_update_repeat(PSX_PAD_DOWN)) {
        if (level->bar.selected_entry < level->bar.entry_count - 1) {
            g_display_object_animated_pool[level->bar.cursor_window].position.y += 0x10;
            level->bar.selected_entry++;
        } else {
            g_display_object_animated_pool[level->bar.cursor_window].position.y -= (level->bar.entry_count - 1) * 0x10;
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
 * variable (SCRIPT_VAR_RUMOR_FIRST + index) is set, listed by name
 * (TEXT_ID_RUMOR_NAME_BASE + index). The panel shows up to eight rows at
 * (0x4c, 0x50), 0x96 wide, and the MENU_STACK_LEVEL_RUMOR_LIST level is pushed.
 * With no rumors the bar menu's windows are reset instead and
 * TEXT_ID_NO_RUMORS is shown. */
static void open_rumor_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* colors;
    s32* locations;
    s32 mask;
    s32 entry;
    s32 count = 0;
    s32 i = 0; /* zeroed ahead of the row pointers, as the target schedules it */
    s32 record_index;

    colors = g_menu_stack_row_text_colors;
    values = g_menu_stack_entry_values;
    locations = g_rumor_location_masks;
    /* The masks hold one bit per town, location 0 in bit 14. */
    mask = 0x4000 >> g_world_map_marker_state.marker.location_id;
    for (; i < SCRIPT_VAR_RUMOR_COUNT; i++, locations++) {
        /* A register operand keeps the loaded 32-bit value first in the `and`;
         * testing *locations directly lets expand_binop swap them. */
        entry = *locations;
        if ((entry & mask) && script_variable_get(i + SCRIPT_VAR_RUMOR_FIRST) != 0) {
            *values = i + TEXT_ID_RUMOR_NAME_BASE;
            *colors = 0;
            colors++;
            values++;
            count++;
        }
    }

    if (count != 0) {
        g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_RUMORS_INTRO, 0);
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
        if (count < 8) {
            i = count;
        } else {
            i = 8;
        }
        layout.text_width = 0x96;
        origin.x = 76;
        layout.visible_rows = i;
        origin.y = 80;
        menu_stack_restore_cursor(MENU_STACK_CURSOR_SLOT_RUMORS, count, i);
        window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
            g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    } else {
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].palette
            = g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.label_window].palette
            = g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].palette = 0;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].sequence
            = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].frame_index
            = g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window]
                  .anim_counter = 0;
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
void rumor_resume_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_RUMORS_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void rumor_suspend_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Pop the rumors list level: toggle the input-blocked system flag and
 * unblock the menu buttons. The caller hands in its menu level; the target
 * reads no argument. */
static void close_rumor_list(menu_stack_panel_level_t* level) {
    g_menu_stack_ot_offset = 1;
    g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
    --g_menu_stack_depth;
    g_menu_blocked_buttons = 0;
}

/* Input step of the bar's rumors list. Cross, or Circle on a non-empty list,
 * closes the panel thread and records the choice in result; Select on a
 * non-empty list opens help message 0x105e. Once the panel thread has ended,
 * the level closes and a chosen rumor's entry value (TEXT_ID_ENTRY_MASK of
 * g_menu_stack_entry_values) goes to open_rumor_detail;
 * otherwise the bar menu resumes.
 *
 * The target passes the level to close_rumor_list and
 * rumor_suspend_list even though neither reads the argument. */
void rumor_update_list_input(menu_stack_record_t* level) {
    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL)) {
        close_rumor_list(&level->panel);
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_RUMORS);
            open_rumor_detail(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_RUMORS);
        g_world_map_flags &= ~WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        rumor_suspend_list(level);
        menu_stack_open_message(0x105E, 1);
        return;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Show rumor `value` in the scrollable text window, titled by the rumor-about message,
 * hide the bar menu's windows and push the rumor detail level, which keeps
 * their indices to show them again. coordinate_mode is always 0, so the
 * switch's mode-1 image row is never used. */
static void open_rumor_detail(s32 value) {
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
        g_text_window.image_position.y = 112;
        break;
    case 0:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 96;
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
    window_open_paged_text(&g_text_window);
    depth = g_menu_stack_depth;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].bar.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_records[depth].rumor_detail.first_window = g_menu_stack_records[depth - 1].bar.cursor_window;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].bar.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_records[depth].rumor_detail.second_window = g_menu_stack_records[depth - 1].bar.label_window;
    g_display_object_gs_image_pool[g_menu_stack_records[depth - 1].bar.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
    g_menu_stack_records[depth].rumor_detail.render_index = g_menu_stack_records[depth - 1].bar.rows_image;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_RUMOR_DETAIL;
}

/* Close the rumor detail level once its text window is finished.
 *
 * Same wait as update_text_level; then shows the bar
 * menu's two windows and frame again, pops the stack and replays the frame's
 * pop-in while open_rumor_list rebuilds the list. */
void rumor_update_detail(menu_stack_record_t* level) {
    if (g_display_object_gs_image_pool[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (window_update_paged_text(&g_text_window) != 0) {
        return;
    }
    g_display_object_animated_pool[level->rumor_detail.first_window].flags ^= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[level->rumor_detail.second_window].flags ^= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[level->rumor_detail.render_index].flags ^= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth--;
    g_display_object_gs_image_pool[level->rumor_detail.render_index].flags |= DISPLAY_OBJECT_POP_IN;
    open_rumor_list();
}

/* Open the bar's list of propositions on offer (the Propositions row of
 * bar_update_menu). If the bar menu is hidden (returning from a
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
static void open_available_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 count;
    s32 i;
    s32 index;
    display_object_animated_t* record;
    s32 record_index;

    if (g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].flags
        & DISPLAY_OBJECT_HIDDEN) {
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.label_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        bar_build_rows(&g_menu_stack_records[g_menu_stack_depth - 1].bar);
        bar_suspend_menu(&g_menu_stack_records[g_menu_stack_depth - 1]);
        index = g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window;
        record = &g_display_object_animated_pool[index];
        record->anim_counter = 0;
        record->frame_index = 0;
        g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_STILL;
        g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].flags
            |= DISPLAY_OBJECT_POP_IN;
    }

    count = 0;
    for (i = 0; i < SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (!(script_variable_get(i + SCRIPT_VAR_PROPOSITION_FIRST)
                & (PROPOSITION_VARIABLE_IN_PROGRESS | PROPOSITION_VARIABLE_COMPLETED))
            && !(g_proposition_states[i] & PROPOSITION_STATE_DAYS_MASK)
            && proposition_is_available_at(i, g_world_map_marker_state.marker.location_id + 1)) {
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
        origin.x = 76;
        layout.visible_rows = i;
        origin.y = 80;
        menu_stack_restore_cursor(MENU_STACK_CURSOR_SLOT_AVAILABLE_PROPOSITIONS, count, i);
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
void proposition_resume_available_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_PROPOSITIONS_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void proposition_suspend_available_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Pop the available propositions list level: toggle the input-blocked system flag and
 * unblock the menu buttons. The caller hands in its menu level; the target
 * reads no argument. */
static void close_available_list(menu_stack_panel_level_t* level) {
    g_menu_stack_ot_offset = 1;
    g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
    --g_menu_stack_depth;
    g_menu_blocked_buttons = 0;
}

/* List level input step for the available propositions (cursor slot 3, Select
 * help 0x105f). Circle hands the chosen entry to
 * open_proposition_detail, but is refused with "I can't give
 * anymore jobs" once g_proposition_count has reached 8. For a
 * proposition already attempted (PROPOSITION_VARIABLE_ATTEMPTED) it
 * first hides the parent level's windows and render record, says "Think I
 * told you before but..." and waits in result 2 for Circle or Cross. */
void proposition_update_available_list_input(menu_stack_record_t* level) {
    s32 depth;

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL)) {
        close_available_list(&level->panel);
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_AVAILABLE_PROPOSITIONS);
            open_proposition_detail(
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
            g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        }
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS)
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        proposition_suspend_available_list(level);
        menu_stack_open_message(0x105F, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        if (g_proposition_count >= 8) {
            sound_play_effect(SOUND_SFX_INVALID);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_NO_MORE_JOBS, 0);
            return;
        }
        sound_play_effect(SOUND_SFX_CONFIRM);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        if (script_variable_get(
                (g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK)
                + SCRIPT_VAR_PROPOSITION_FIRST)
            & PROPOSITION_VARIABLE_ATTEMPTED) {
            depth = g_menu_stack_depth;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].bar.cursor_window].flags
                |= DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].bar.label_window].flags
                |= DISPLAY_OBJECT_HIDDEN;
            g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].bar.rows_image].flags
                |= DISPLAY_OBJECT_HIDDEN;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TOLD_BEFORE, 0);
            level->panel.result = 2;
            return;
        }
        level->panel.result = 1;
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Push the proposition detail level and show its description; the bar owner first gives his advice
 * on it with a 10% chance (20% after a failure, 40% after a bad failure), otherwise says 'This's
 * what I know about ...'. */
static void open_proposition_detail(s32 value) {
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
    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_detail.render_index = index;
    bounds.y = 96;
    bounds.w = 244;
    bounds.h = 128;
    bounds.x = 0;
    window_init_text_record(index, bounds, 0, value + TEXT_ID_WORLD_MESSAGE_BASE, g_window_image_buffer);
    base = &g_display_object_gs_image_pool[index].position;
    g_display_object_gs_image_pool[index].priority = 9;
    depth = g_menu_stack_depth;
    base->x = -0x7A;
    base->y = -0x28;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].bar.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].bar.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[depth - 1].bar.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_DETAIL;
}

/* Restore the proposition detail renderer's palette. */
void proposition_resume_detail(menu_stack_record_t* level) {
    g_display_object_gs_image_pool[level->proposition_detail.render_index].palette = 0;
}

/* Select palette 2 for the window's render record. */
void proposition_suspend_detail(menu_stack_record_t* pair) {
    g_display_object_gs_image_pool[pair->proposition_detail.render_index].palette = 2;
}

/* Input step of the proposition detail level. Cross closes it and reopens the
 * list (open_available_list). Circle stores the job's
 * total fee (fee_a + fee_b of the selected row) as the gil to pay, asks
 * TEXT_ID_JOB_FEE_PROMPT with the fee substituted (or
 * TEXT_ID_ACCEPT_JOB_PROMPT when it is free), opens the OK/NO panel
 * at (0x48, 0x20) and pushes MENU_STACK_LEVEL_PROPOSITION_ACCEPT. */
void proposition_update_detail(menu_stack_record_t* owner) {
    s32 total;
    point32_t origin;

    if (g_display_object_gs_image_pool[owner->proposition_detail.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_display_object_image_count--;
        g_display_object_queue_count--;
        g_menu_stack_depth--;
        open_available_list();
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
        proposition_suspend_detail(owner);
        origin.x = 72;
        origin.y = 32;
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

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].accept_confirm.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].accept_confirm.header_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CHOICE_PANEL_HEADER;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].accept_confirm.render_index = index;
    bounds.x = 128;
    bounds.w = 40;
    bounds.h = 48;
    bounds.y = 0;
    window_init_text_record(index, bounds, 0, TEXT_ID_OK_NO, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 8;
    g_display_object_gs_image_pool[index].position = origin;

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].accept_confirm.row = 0;
    g_menu_stack_records[depth].accept_confirm.mode = PROPOSITION_ACCEPT_MODE_CHOOSE;
    g_display_object_animated_pool[g_menu_stack_records[depth].accept_confirm.cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 6;
    g_display_object_animated_pool[g_menu_stack_records[depth].accept_confirm.cursor_window].position.y
        = g_display_object_gs_image_pool[index].position.y
        - -(g_menu_stack_records[depth].accept_confirm.row * 0x10 + 0xE);
    g_display_object_animated_pool[g_menu_stack_records[depth].accept_confirm.header_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 3;
    g_display_object_animated_pool[g_menu_stack_records[depth].accept_confirm.header_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
}

/* Input step of the proposition yes/no confirmation panel.
 *
 * Up/down toggle the highlighted row and move the cursor window by one
 * 16-pixel row. Cross and NO both tear the panel down and return through
 * open_available_list. OK first checks that at least one
 * generic, non-monster party member is free (the same filter as
 * build_candidates) and that the war funds cover
 * g_proposition_gil_amount. With no unit it says
 * TEXT_ID_CHOOSE_MORE_UNITS, then TEXT_ID_CHOOSE_HUMAN_UNIT
 * after a button (NO_UNIT_MESSAGE mode); short of gil it says
 * TEXT_ID_NOT_ENOUGH_GIL; either way the next button closes the panel
 * (CLOSE_MESSAGE mode). A successful confirmation continues into
 * open_participants.
 *
 * The gotos are the target's shared tails: the mode 3 close, the "no"
 * dismissal and the no-unit failure jump into the cancel and gil-failure
 * blocks; duplicated tails do not cross-jump back into that shape. */
void proposition_update_accept_panel(menu_stack_record_t* level) {
    s32 available;
    s32 i;
    s32 row;
    party_unit_t* unit;

    if (g_display_object_gs_image_pool[level->accept_confirm.render_index].flags & DISPLAY_OBJECT_POP_IN) {
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
        g_display_object_animated_count -= 2;
        g_display_object_image_count--;
        g_display_object_queue_count -= 3;
    close:
        g_display_object_image_count--;
        g_display_object_queue_count--;
        g_menu_stack_depth -= 2;
        open_available_list();
        return;
    }

    if (input_update_repeat(PSX_PAD_UP) || input_update_repeat(PSX_PAD_DOWN)) {
        row = level->accept_confirm.row ^ 1;
        level->accept_confirm.row = row;
        g_display_object_animated_pool[level->accept_confirm.cursor_window].position.y
            = g_display_object_gs_image_pool[level->accept_confirm.render_index].position.y - -(row * 0x10 + 0xE);
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

    if (script_variable_get(SCRIPT_VAR_WAR_FUNDS) < g_proposition_gil_amount) {
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_NOT_ENOUGH_GIL, 0);
        level->accept_confirm.mode = PROPOSITION_ACCEPT_MODE_CLOSE_MESSAGE;
    fail:
        sound_play_effect(SOUND_SFX_INVALID);
        g_display_object_animated_count -= 2;
        g_display_object_image_count--;
        g_display_object_queue_count -= 3;
        return;
    }

    sound_play_effect(SOUND_SFX_CONFIRM);
    g_display_object_animated_count -= 2;
    g_display_object_image_count--;
    g_display_object_queue_count -= 3;
    g_display_object_image_count--;
    g_display_object_queue_count--;
    g_menu_stack_depth -= 2;
    open_participants();
}

/* Pushes the "how many days" number panel of a proposition: shows
 * TEXT_ID_CHOOSE_DAYS with the row's minimum and maximum days,
 * starts the number panel thread on that range at the minimum, and withholds
 * Circle and Cross from the WORLD menu threads (the level's own step handles
 * them). */
static void open_days_panel(void) {
    point32_t origin;
    s32 record_index;
    s32 min_days = g_proposition_selected_row.min_days;
    s32 max_days = g_proposition_selected_row.max_days;

    g_text_substitution_values[0] = min_days;
    g_text_substitution_values[1] = max_days;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_DAYS, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_menu_stack_ot_offset = 8;
    origin.x = 56;
    origin.y = 32;
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
void proposition_update_days_panel(menu_stack_record_t* level) {
    if (level->panel.result != 0 && !thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL)) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_display_object_image_count--;
        g_display_object_queue_count--;
        g_menu_stack_depth -= 2;
        if (level->panel.result == 1) {
            open_dispatch_confirm();
        } else {
            open_available_list();
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

/* dispatch */

static s32 s_proposition_send_unit_count;
extern s16 g_proposition_candidate_levels[];

/* Scalar view of g_window_panel_render_state.select_text_table: as a
 * struct field its address would share the selected_index base register, but
 * the target stores it by symbol. */
extern s16 g_window_panel_scroll_offset;

static s32 s_proposition_send_units[3] = { 0 };

/* Pushes MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM, the summary of a job
 * about to be dispatched: a cursor window, a header window, the OK/NO record
 * and a summary record with "Job contents:" and the job's contents text
 * (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1 + name_text), the days and place
 * labels (TEXT_ID_JOB_CONDITION_LABELS) and one row per chosen unit.
 * The message box asks TEXT_ID_CONFIRM_CONDITIONS; both windows are
 * placed against the OK/NO record.
 *
 * `unused` reproduces the target's 0x78-byte frame. The row offset uses its
 * own local: reusing `row` (then set twice) schedules the loop's `addiu v1`
 * ahead of `move a0`. */
static void open_dispatch_confirm(void) {
    RECT bounds;
    point32_t point;
    text_dimensions_t size;
    s32 unused[8];
    point32_t* position;
    point32_t* base;
    s32 index;
    s32 depth;
    s32 i;
    s32 row;
    s32 line;

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CONFIRM_CONDITIONS, 0);

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.header_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CHOICE_PANEL_HEADER;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.render_index = index;
    bounds.x = 128;
    bounds.w = 40;
    bounds.h = 48;
    bounds.y = 0;
    window_init_text_record(index, bounds, 0, TEXT_ID_OK_NO, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 8;
    position = &g_display_object_gs_image_pool[index].position;
    position->x = 0x48;
    position->y = 0x20;

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    /* The summary panel's render record index lands in the 32-bit row field, which is cleared below. */
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.row = index;
    text_measure_pixels(&size.width, &size.rows,
        text_find_entry(g_proposition_selected_row.name_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    bounds.y = 96;
    bounds.w = 244;
    bounds.x = 0;
    row = size.rows + 2;
    bounds.h = (s_proposition_send_unit_count - -row) * 0x10 + 0x10;
    g_text_substitution_values[1] = g_proposition_dispatch_days;
    g_text_substitution_values[0] = g_proposition_dispatch_days;
    g_text_substitution_values[2] = g_world_map_marker_state.marker.location_id + TEXT_ID_LOCATION_NAME_BASE;
    window_build_render_record_image(index, bounds, 0, &g_window_image_buffer[0x200]);
    point.x = 8;
    point.y = size.rows * 0x10 + 8;
    window_render_text_entry(index, TEXT_ID_JOB_CONDITION_LABELS, point, &g_window_image_buffer[0x200]);
    point.x = 8;
    point.y = 8;
    window_render_text_entry(index, TEXT_ID_JOB_CONTENTS_LABEL, point, &g_window_image_buffer[0x200]);
    point.x = 68;
    point.y = 8;
    window_render_text_entry(index, g_proposition_selected_row.name_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1),
        point, &g_window_image_buffer[0x200]);
    for (i = 0; i < s_proposition_send_unit_count; i++) {
        point.x = 60;
        line = i + 2;
        point.y = (size.rows + line) * 0x10 + 8;
        window_render_text_entry(
            index, s_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point, &g_window_image_buffer[0x200]);
    }
    window_load_render_record_image(index, &g_window_image_buffer[0x200]);
    g_display_object_gs_image_pool[index].priority = 8;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_display_object_gs_image_pool[index].position;
    base->x = -0x7A;
    base->y = -0x28;

    depth = g_menu_stack_depth;
    index = g_menu_stack_records[depth].dispatch_confirm.render_index;
    g_menu_stack_records[depth].dispatch_confirm.row = 0;
    g_display_object_animated_pool[g_menu_stack_records[depth].dispatch_confirm.cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 6;
    g_display_object_animated_pool[g_menu_stack_records[depth].dispatch_confirm.cursor_window].position.y
        = g_display_object_gs_image_pool[index].position.y
        - -(g_menu_stack_records[depth].dispatch_confirm.row * 0x10 + 0xE);
    g_display_object_animated_pool[g_menu_stack_records[depth].dispatch_confirm.header_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 3;
    g_menu_stack_depth = depth + 1;
    g_display_object_animated_pool[g_menu_stack_records[depth].dispatch_confirm.header_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM;
}

/* Input step of the dispatch confirmation's OK/NO panel: up/down toggle the
 * row, Cross and NO close the panel; OK pays g_proposition_gil_amount
 * from the war funds, marks the proposition attempted and in progress,
 * records it in the next g_proposition_active slot with its
 * participants (each marked busy), and closes the panel. Either way the
 * propositions list reopens. */
void proposition_update_dispatch_confirm(menu_stack_record_t* level) {
    s32 row;
    s32 i;

    if (g_display_object_gs_image_pool[level->dispatch_confirm.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and both confirm paths
         * jump into it; duplicated copies cross-jump into the last one. */
    block_tail:
        g_hud_value_mode = HUD_VALUE_FUNDS;
        g_display_object_animated_count -= 2;
        g_display_object_image_count -= 2;
        g_display_object_queue_count -= 4;
        g_menu_stack_depth -= 1;
        open_available_list();
        return;
    }

    if (input_update_repeat(PSX_PAD_UP) || input_update_repeat(PSX_PAD_DOWN)) {
        row = level->dispatch_confirm.row ^ 1;
        level->dispatch_confirm.row = row;
        g_display_object_animated_pool[level->dispatch_confirm.cursor_window].position.y
            = g_display_object_gs_image_pool[level->dispatch_confirm.render_index].position.y - -(row * 0x10 + 0xE);
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        if (level->dispatch_confirm.row != 1) {
            script_variable_set(
                SCRIPT_VAR_WAR_FUNDS, script_variable_get(SCRIPT_VAR_WAR_FUNDS) - g_proposition_gil_amount);
            g_hud_value = script_variable_get(SCRIPT_VAR_WAR_FUNDS);
            script_variable_set(g_proposition_selected_row.id + (SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_variable_get(g_proposition_selected_row.id + (SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | PROPOSITION_VARIABLE_ATTEMPTED);
            g_proposition_active[g_proposition_count].flags = PROPOSITION_PROGRESS_DISPATCHED;
            g_proposition_active[g_proposition_count].proposition_id = g_proposition_selected_row.id - 1;
            g_proposition_active[g_proposition_count].elapsed_days = 0;
            g_proposition_active[g_proposition_count].assigned_days = g_proposition_dispatch_days;
            g_proposition_active[g_proposition_count].location = g_world_map_marker_state.marker.location_id;
            g_proposition_active[g_proposition_count].participant_count = s_proposition_send_unit_count;
            for (i = 0; i < s_proposition_send_unit_count; i++) {
                party_get_roster_unit(s_proposition_send_units[i])->proposition_status = 1;
                g_proposition_active[g_proposition_count].participant_indices[i] = s_proposition_send_units[i];
            }
            g_proposition_count++;
            script_variable_set(g_proposition_selected_row.id + (SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_variable_get(g_proposition_selected_row.id + (SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | PROPOSITION_VARIABLE_IN_PROGRESS);
        }
        goto block_tail;
    }
}

/*
 * Advance daily proposition progress and packed countdown bytes.
 *
 * Every active proposition not yet returned counts one more elapsed day, and
 * returns (PROPOSITION_PROGRESS_RETURNED) when that reaches its
 * assigned days or it was recalled. Each proposition's days until it is
 * offered again drop by one, from at most 32, keeping its failure bits.
 */
void proposition_advance_day(void) {
    s32 i;

    for (i = 0; i < g_proposition_count; i++) {
        if (!(g_proposition_active[i].flags & PROPOSITION_PROGRESS_RETURNED)) {
            g_proposition_active[i].elapsed_days++;
            if (g_proposition_active[i].assigned_days == g_proposition_active[i].elapsed_days
                || (g_proposition_active[i].flags & PROPOSITION_PROGRESS_RECALLED)) {
                g_proposition_active[i].flags |= PROPOSITION_PROGRESS_RETURNED;
            }
        }
    }
    for (i = 0; i < SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        u32 value = g_proposition_states[i];
        s32 days = value & PROPOSITION_STATE_DAYS_MASK;
        if (days != 0) {
            if (days > 32) {
                days = 32;
            }
            days--;
            g_proposition_states[i] = (value & (PROPOSITION_STATE_FAILURE | PROPOSITION_STATE_BAD_FAILURE)) | days;
        }
    }
}

/*
 * Count the active propositions that have returned
 * (PROPOSITION_PROGRESS_RETURNED) to a location, or to any location
 * when `location` is -1.
 */
static s32 count_returned_propositions(s32 location) {
    s32 i;
    s32 count = 0;
    for (i = 0; i < g_proposition_count; i++) {
        if (g_proposition_active[i].flags & PROPOSITION_PROGRESS_RETURNED) {
            if (location == -1 || g_proposition_active[i].location == location)
                count++;
        }
    }
    return count;
}

/* Push the send-unit level (MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT).
 *
 * Builds the available-unit list into the new level record, starts the panel
 * thread on it with each unit's level as a second column, appends the level's
 * seven window records and five render records, and asks
 * TEXT_ID_CHOOSE_UNITS.
 *
 * The selected index is written through the panel render state:
 * the target derives the thread parameter from that field's address. Each
 * record's x/y pair is written through a block-local pointer so the pointer
 * shares the index product's register, and the chained frame_index/anim_counter
 * store keeps anim_counter's base in a register as in the target. */
static void open_participants(void) {
    window_panel_layout_t layout;
    point32_t origin;
    RECT bounds;
    s32 index;
    s32 depth;
    s32 count;

    g_window_panel_scroll_offset = 0;
    g_window_panel_render_state.selected_index = 0;
    s_proposition_send_unit_count = 0;
    count = build_candidates(&g_menu_stack_records[g_menu_stack_depth].send_unit);

    layout.text_width = 0x6E;
    layout.visible_rows = count;
    origin.x = 8;
    origin.y = 80;
    window_init_panel_state(0xC, g_menu_stack_records[g_menu_stack_depth].send_unit.unit_count, &layout, &origin);

    /* The initializer hides label column 1; show it with each candidate's level as digits. */
    g_window_panel_label_state.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_window_panel_label_state.x[1] = 0x58;
    g_window_panel_label_state.text_ids[1] = g_proposition_candidate_levels;
    g_window_panel_label_state.text_colors[1] = g_menu_stack_row_text_colors;
    g_window_panel_label_state.x[0] = 0;
    g_window_panel_render_state.header_id = 0x10;
    g_window_panel_render_state.selected_index = 0;
    g_window_panel_scroll_offset = 0;
    thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, menu_screen_text_list_thread);
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, (s32)&g_window_panel_render_state, 0, 0);

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.list_frame.window_index = index;
    {
        point32_t* position = &g_display_object_animated_pool[index].position;
        g_display_object_animated_pool[index].priority = 9;
        position->x = 0xE;
        position->y = -0x29;
    }
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_PROPOSITION_COUNT_LABEL;
    g_display_object_animated_pool[index].frame_index = g_display_object_animated_pool[index].anim_counter = 0;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 9;
    g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[index].frame_index = g_display_object_animated_pool[index].anim_counter = 0;

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.list_frame.render_index = index;
    bounds.x = 128;
    bounds.y = 0;
    bounds.w = 108;
    bounds.h = 64;
    window_init_text_record(index, bounds, 0, 0, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 9;
    {
        point32_t* base = &g_display_object_gs_image_pool[index].position;
        base->x = 0xC;
        base->y = -0x28;
    }
    DrawSync(0);

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.frame_window_a = index;
    {
        point32_t* position = &g_display_object_animated_pool[index].position;
        g_display_object_animated_pool[index].priority = 9;
        position->x = -0x60;
        position->y = 0x60;
    }
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SEND_UNIT_FRAME_A;
    g_display_object_animated_pool[index].frame_index = g_display_object_animated_pool[index].anim_counter = 0;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.frame_window_b = index;
    {
        point32_t* position = &g_display_object_animated_pool[index].position;
        g_display_object_animated_pool[index].priority = 9;
        position->x = -0x12;
        position->y = 0x60;
    }
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SEND_UNIT_FRAME_B;
    g_display_object_animated_pool[index].frame_index = g_display_object_animated_pool[index].anim_counter = 0;

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.preview_render = index;
    g_display_object_gs_image_pool[index].priority = 9;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    {
        point32_t* base = &g_display_object_gs_image_pool[index].position;
        base->x = 0x11;
        base->y = 0x18;
    }

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.list_render = index;
    bounds.x = 128;
    bounds.y = 64;
    bounds.w = 48;
    bounds.h = 59;
    window_init_text_record(index, bounds, 0, 0, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 9;
    {
        point32_t* base = &g_display_object_gs_image_pool[index].position;
        base->x = 0xC;
        base->y = 0x14;
    }
    DrawSync(0);

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.portrait_render = index;
    g_display_object_gs_image_pool[index].priority = 9;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    {
        point32_t* base = &g_display_object_gs_image_pool[index].position;
        base->x = 0x4D;
        base->y = 0x18;
    }

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.portrait_frame = index;
    bounds.x = 128;
    bounds.y = 64;
    bounds.w = 48;
    bounds.h = 59;
    window_init_text_record(index, bounds, 0, 0, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 9;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    {
        point32_t* base = &g_display_object_gs_image_pool[index].position;
        base->x = 0x48;
        base->y = 0x14;
    }

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.portrait_window = index;
    {
        point32_t* position = &g_display_object_animated_pool[index].position;
        g_display_object_animated_pool[index].priority = 9;
        g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
        position->x = 0x3E;
        position->y = 0x28;
    }
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SEND_UNIT_PORTRAIT;
    g_display_object_animated_pool[index].frame_index = g_display_object_animated_pool[index].anim_counter = 0;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.left_arrow = index;
    {
        point32_t* position = &g_display_object_animated_pool[index].position;
        g_display_object_animated_pool[index].priority = 9;
        g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
        position->x = -0x74;
        position->y = -0x69;
    }
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_L1_BUTTON;
    g_display_object_animated_pool[index].frame_index = g_display_object_animated_pool[index].anim_counter = 0;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.right_arrow = index;
    {
        point32_t* position = &g_display_object_animated_pool[index].position;
        g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
        g_display_object_animated_pool[index].priority = 9;
        g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
        position->x = 0x60;
        position->y = -0x69;
    }
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_R1_BUTTON;
    g_display_object_animated_pool[index].frame_index = g_display_object_animated_pool[index].anim_counter = 0;

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_UNITS, 0);

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].send_unit.shown_preview = -1;
    g_menu_stack_records[depth].send_unit.mode = PROPOSITION_SEND_UNIT_MODE_SELECT;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT;
    g_menu_stack_depth = depth + 1;
}

/* Build the list of available non-guest human units for a proposition.
 *
 * Units already assigned to a proposition, absent roster entries, monsters,
 * and special-character sprite sets are excluded. */
static s32 build_candidates(proposition_send_unit_level_t* level) {
    s32 i;
    s32 j;
    party_unit_t* unit;

    level->unit_count = 0;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        unit = party_get_roster_unit(i);
        if (unit->proposition_status != 0) {
            continue;
        }
        if (unit->party_id == PARTY_ID_NONE) {
            continue;
        }
        if (unit->gender_flags & UNIT_FLAG_MONSTER) {
            continue;
        }
        if (unit->sprite_set < CHARACTER_IDENTITY_BOY) {
            continue;
        }
        for (j = 0; j < s_proposition_send_unit_count; j++) {
            if (s_proposition_send_units[j] == i) {
                break;
            }
        }
        if (j < s_proposition_send_unit_count) {
            g_menu_stack_row_text_colors[level->unit_count] = 8;
        } else {
            g_menu_stack_row_text_colors[level->unit_count] = 0;
        }
        g_menu_stack_entry_values[level->unit_count] = i + TEXT_ID_UNIT_NAME_BASE;
        g_proposition_candidate_levels[level->unit_count] = unit->level;
        level->unit_count++;
    }
    if (level->unit_count < 6) {
        i = level->unit_count;
    } else {
        i = 6;
    }
    return i;
}

/* Redraw the send-unit list window: the empty frame, then one name row per unit chosen so far. */
static void draw_participant_names(proposition_send_unit_level_t* level) {
    point32_t point;
    s32 index = level->list_frame.render_index;
    s32 i;

    menu_screen_build_window_frame_image(g_display_object_gs_image_pool[index].bounds.w,
        g_display_object_gs_image_pool[index].bounds.h, (u16*)g_window_image_buffer);
    point.x = 8;
    point.y = 8;
    for (i = 0; i < s_proposition_send_unit_count; i++) {
        window_render_text_entry(
            index, s_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point, g_window_image_buffer);
        point.y += 0x10;
    }
    window_load_render_record_image(index, g_window_image_buffer);
}

/* Point render record `index` at the formation portrait of party member
 * `party_index`: its texture page, cell rectangle and CLUT (16 colors at
 * x 0x280 + 0x40 per 40 portraits + 16 * (code % 4), y 0x1F0 +
 * (code % 40) / 4), at full brightness, and show the record. The first
 * argument is not read. */
static void show_candidate_portrait(proposition_send_unit_level_t* unused, s32 party_index, s32 index) {
    POLY_FT4 poly;
    s32 code;
    s32 cell;
    CVECTOR* color;
    RECT* bounds;

    code = portrait_get_party_unit_code(party_index);
    cell = code;
    portrait_build_wldface_poly(&poly, code);
    g_display_object_gs_image_pool[index].clut.x = (code / 40 << 6) + 0x280 + cell % 4 * 16;
    g_display_object_gs_image_pool[index].clut.y = cell % 40 / 4 + 0x1F0;
    g_display_object_gs_image_pool[index].tpage = poly.tpage;
    bounds = &g_display_object_gs_image_pool[index].bounds;
    bounds->x = poly.u0;
    bounds->y = poly.v0;
    bounds->w = poly.u1 - poly.u0;
    bounds->h = poly.v2 - poly.v0;
    color = &g_display_object_gs_image_pool[index].color;
    color->b = color->g = color->r = 0x80;
    g_display_object_gs_image_pool[index].pop_in_step = 0;
    g_display_object_gs_image_pool[index].flags &= ~DISPLAY_OBJECT_HIDDEN;
}

/* Input step of the proposition send-unit level.
 *
 * While the portrait frame is visible, a changed unit in the selected slot is
 * redrawn into the portrait. Cross and Circle both leave slot editing; Circle
 * first stores the highlighted list unit into the slot, redraws the names,
 * rebuilds the available-unit list and asks the panel thread to redraw. Up
 * and down wrap the slot through 0..2 and move the cursor window.
 *
 * The target's frame holds two 32-bit values no instruction touches, reproduced by
 * the unused point. */
static void update_replace_mode(proposition_send_unit_level_t* level) {
    point32_t unused;
    u32 buttons;
    s32 unit;

    if (!(g_display_object_gs_image_pool[level->portrait_frame].flags & DISPLAY_OBJECT_POP_IN)) {
        unit = s_proposition_send_units[level->slot] & 0x7FF;
        if (unit != level->shown_unit) {
            level->shown_unit = unit;
            show_candidate_portrait(level, unit, level->portrait_render);
        }
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        /* Shared tail: a duplicated close path cross-jumps to the same
         * layout, but without this label the scheduler reorders its loads. */
        goto close;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        unit = g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
        s_proposition_send_units[level->slot] = unit;
        draw_participant_names(level);
        build_candidates(level);
        menu_screen_thread_request_redraw(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL);
    close:
        g_display_object_animated_pool[level->frame_window_a].palette
            = g_display_object_animated_pool[level->frame_window_b].palette = 0;
        g_display_object_gs_image_pool[level->portrait_render].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[level->portrait_frame].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->portrait_window].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
        g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
        level->mode = PROPOSITION_SEND_UNIT_MODE_SELECT;
        return;
    }

    if (input_update_repeat(PSX_PAD_UP)) {
        if (level->slot == 0) {
            level->slot = 2;
        } else {
            level->slot--;
        }
    } else if (input_update_repeat(PSX_PAD_DOWN)) {
        if (level->slot == 2) {
            level->slot = 0;
        } else {
            level->slot++;
        }
    } else {
        return;
    }

    sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    g_display_object_animated_pool[level->cursor_window].position.x
        = g_display_object_gs_image_pool[level->list_frame.render_index].position.x + 6;
    g_display_object_animated_pool[level->cursor_window].position.y
        = g_display_object_gs_image_pool[level->list_frame.render_index].position.y + level->slot * 16 + 12;
}

/* Build the status panel data of party member `party_index` and start the
 * three unit panel threads on it (status banner, numeric panel, equipment
 * and ability panel), raising WORLD_MAP_FLAG_UNIT_PANEL_THREADS. */
static void start_candidate_panels(s32 party_index) {
    unit_panel_thread_t* banner;

    unit_panel_prepare_party_unit(party_index);
    banner = &g_unit_panel_banner_params;
    g_unit_panel_numbers_params.position.y = 0;
    g_unit_panel_panel_params.position.y = 0;
    g_unit_panel_banner_params.position.y = 0;
    banner->position.x = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)banner, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_panel_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_unit_panel_numbers_params, 0, 0);
    g_world_map_flags |= WORLD_MAP_FLAG_UNIT_PANEL_THREADS;
}

/* Status view of the send-unit level: once the panel threads stop it opens the unit's status
 * screens; L1 and R1 page through the candidates with wraparound, Select opens the layout window,
 * Circle or Cross close the view and return to 'So, who you wanna send?'. */
static void update_status_mode(proposition_send_unit_level_t* level) {
    u32 buttons;
    s32 selected;
    s32 direction;

    if (level->mode == PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL)) {
            return;
        }
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE)) {
            return;
        }
        g_display_object_animated_pool[level->left_arrow].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->right_arrow].flags ^= DISPLAY_OBJECT_HIDDEN;
        level->mode = PROPOSITION_SEND_UNIT_MODE_STATUS;
        start_candidate_panels(
            g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
        level->slot = g_window_panel_render_state.selected_index;
    } else if (level->mode == PROPOSITION_SEND_UNIT_MODE_STATUS) {
        buttons = g_world_map_input_new_button_presses;
        if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_display_object_animated_pool[level->left_arrow].flags |= DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[level->right_arrow].flags |= DISPLAY_OBJECT_HIDDEN;
            level->mode = PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE;
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_SELECT) {
            g_display_object_animated_pool[level->left_arrow].palette
                = g_display_object_animated_pool[level->right_arrow].palette = 0xA;
            sound_play_effect(SOUND_SFX_CONFIRM);
            unit_panel_open_help();
            return;
        }
        if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_display_object_animated_pool[level->left_arrow].sequence = DISPLAY_OBJECT_ANIM_L1_BUTTON;
            g_display_object_animated_pool[level->right_arrow].sequence = DISPLAY_OBJECT_ANIM_R1_BUTTON;
            g_display_object_animated_pool[level->left_arrow].frame_index
                = g_display_object_animated_pool[level->left_arrow].anim_counter
                = g_display_object_animated_pool[level->right_arrow].frame_index
                = g_display_object_animated_pool[level->right_arrow].anim_counter = 0;
        }
        if (input_update_repeat(PSX_PAD_R1)
            && g_display_object_animated_pool[level->left_arrow].sequence != DISPLAY_OBJECT_ANIM_L1_BUTTON_PRESS) {
            g_display_object_animated_pool[level->right_arrow].sequence = DISPLAY_OBJECT_ANIM_R1_BUTTON_PRESS;
            g_display_object_animated_pool[level->right_arrow].frame_index
                = g_display_object_animated_pool[level->right_arrow].anim_counter = 0;
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
            if (!input_update_repeat(PSX_PAD_L1)
                || g_display_object_animated_pool[level->right_arrow].sequence == DISPLAY_OBJECT_ANIM_R1_BUTTON_PRESS) {
                return;
            }
            g_display_object_animated_pool[level->left_arrow].sequence = DISPLAY_OBJECT_ANIM_L1_BUTTON_PRESS;
            g_display_object_animated_pool[level->left_arrow].frame_index
                = g_display_object_animated_pool[level->left_arrow].anim_counter = 0;
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
        g_unit_panel_banner_params.redraw_request = direction;
        unit_panel_prepare_party_unit(g_menu_stack_entry_values[level->slot] & TEXT_ID_ENTRY_MASK);
        g_unit_panel_panel_params.redraw_request = 1;
        g_unit_panel_numbers_params.redraw_request = 1;
    done:
        sound_play_effect(SOUND_SFX_PAGE_SWITCH);
    } else if (level->mode == PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE) {
        if (thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS)) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_PANEL)) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS)) {
            return;
        }
        level->mode = PROPOSITION_SEND_UNIT_MODE_SELECT;
        g_world_map_flags &= ~(WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE | WORLD_MAP_FLAG_UNIT_PANEL_THREADS);
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_UNITS, 0);
        thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, menu_screen_text_list_thread);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, (s32)&g_window_panel_render_state, 0, 0);
        g_display_object_animated_pool[level->list_frame.window_index].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[level->list_frame.render_index].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->frame_window_a].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->frame_window_b].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[level->preview_render].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[level->list_render].flags ^= DISPLAY_OBJECT_HIDDEN;
    }
}

/* Undo proposition_suspend_participants when a sub-level closes:
 * default palettes on the send-unit windows and a neutral preview portrait. */
void proposition_resume_participants(menu_stack_record_t* level) {
    CVECTOR* color;

    g_display_object_animated_pool[level->send_unit.list_frame.window_index].palette
        = g_display_object_gs_image_pool[level->send_unit.list_frame.render_index].palette
        = g_display_object_gs_image_pool[level->send_unit.list_render].palette
        = g_display_object_animated_pool[level->send_unit.cursor_window].palette = 0;
    /* Each channel store reloads the level's render index. */
    color = &g_display_object_gs_image_pool[level->send_unit.preview_render].color;
    color->r = 0x80;
    color = &g_display_object_gs_image_pool[level->send_unit.preview_render].color;
    color->g = 0x80;
    color = &g_display_object_gs_image_pool[level->send_unit.preview_render].color;
    color->b = 0x80;
    g_display_object_animated_pool[level->send_unit.left_arrow].palette
        = g_display_object_animated_pool[level->send_unit.right_arrow].palette = 0;
}

/* Gray out the send-unit windows (palettes 6, 10 and 2) and tint the preview
 * portrait blue (0x40, 0x50, 0x70) while a sub-level runs over them; undone by
 * proposition_resume_participants. */
void proposition_suspend_participants(menu_stack_record_t* level) {
    s32 pad[4];

    g_display_object_animated_pool[level->send_unit.list_frame.window_index].palette = 6;
    g_display_object_gs_image_pool[level->send_unit.list_frame.render_index].palette = 2;
    g_display_object_gs_image_pool[level->send_unit.list_render].palette = 2;
    g_display_object_animated_pool[level->send_unit.cursor_window].palette = 10;
    g_display_object_gs_image_pool[level->send_unit.preview_render].color.r = 0x40;
    g_display_object_gs_image_pool[level->send_unit.preview_render].color.g = 0x50;
    g_display_object_gs_image_pool[level->send_unit.preview_render].color.b = 0x70;
}

/* Top-level step of the proposition send-unit menu level, dispatching on the
 * level's mode.
 *
 * CANCEL and ACCEPT wait for the panel thread to close and then pop the level,
 * ACCEPT continuing into the proposition detail level and its days panel; the
 * STATUS modes run update_status_mode and REPLACE runs
 * update_replace_mode.
 *
 * Otherwise (SELECT) the highlighted unit's portrait is shown in the preview
 * record once the list has popped in, then the new button presses are
 * handled: Select opens help message 0x109a, Start (with at least one chosen
 * unit) accepts, Cross cancels, Square hides the list and opens the unit's
 * status panels, and Circle toggles the highlighted unit in or out of the
 * three g_proposition_send_units slots (removing it compacts the
 * list; adding one when all three are full switches to REPLACE).
 *
 * The target reserves a 0x30-byte frame but only saves s0 and ra; the unused
 * 16-byte array reproduces that frame. The removal and addition paths each
 * end in their own redraw calls, which jump2 cross-jumps from the count
 * store onward. */
void proposition_update_participants(menu_stack_record_t* level) {
    s32 buttons;
    s32 value;
    s32 i;
    s32 unused[4];

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_CANCEL) {
        if (!thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL)) {
            g_menu_blocked_buttons = 0;
            g_world_map_flags &= ~WORLD_MAP_FLAG_INPUT_BLOCKED;
            g_menu_stack_depth--;
            open_available_list();
        }
        return;
    }

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_ACCEPT) {
        if (!thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL)) {
            g_menu_blocked_buttons = 0;
            g_world_map_flags &= ~WORLD_MAP_FLAG_INPUT_BLOCKED;
            g_menu_stack_depth--;
            open_proposition_detail(g_proposition_selected_row.id - 1);
            proposition_suspend_detail(level);
            open_days_panel();
        }
        return;
    }

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN
        || level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_STATUS
        || level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE) {
        update_status_mode(&level->send_unit);
        return;
    }

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_REPLACE) {
        update_replace_mode(&level->send_unit);
        return;
    }

    if (!(g_display_object_gs_image_pool[level->send_unit.list_frame.render_index].flags & DISPLAY_OBJECT_POP_IN)) {
        value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
        if (value != level->send_unit.shown_preview) {
            level->send_unit.shown_preview = value;
            show_candidate_portrait(&level->send_unit, value, level->send_unit.preview_render);
        }
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_SELECT) {
        proposition_suspend_participants(level);
        menu_stack_open_message(0x109A, 1);
        return;
    }

    if (buttons & PSX_PAD_START) {
        if (s_proposition_send_unit_count == 0) {
            sound_play_effect(SOUND_SFX_INVALID);
            return;
        }
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_ACCEPT;
        g_hud_value_mode = HUD_VALUE_COST_AND_FUNDS;
        g_hud_value_secondary = g_proposition_gil_amount;
        /* Shared tail: a duplicated copy is scheduled differently and
         * only partly cross-jumped. */
        goto suspend;
    }

    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_CANCEL;
    suspend:
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_display_object_animated_count -= 7;
        g_display_object_image_count -= 5;
        g_display_object_queue_count -= 0xC;
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        return;
    }

    if (buttons & PSX_PAD_SQUARE) {
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_display_object_animated_pool[level->send_unit.list_frame.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[level->send_unit.list_frame.render_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->send_unit.frame_window_a].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->send_unit.frame_window_b].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[level->send_unit.preview_render].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[level->send_unit.list_render].flags |= DISPLAY_OBJECT_HIDDEN;
        g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN;
        return;
    }

    if (!(buttons & PSX_PAD_CIRCLE)) {
        return;
    }

    sound_play_effect(SOUND_SFX_CONFIRM);
    value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
    for (i = 0; i < s_proposition_send_unit_count; i++) {
        if (s_proposition_send_units[i] == value) {
            for (value = i + 1; value < 3; value++) {
                s_proposition_send_units[value - 1] = s_proposition_send_units[value];
            }
            s_proposition_send_unit_count--;
            draw_participant_names(&level->send_unit);
            build_candidates(&level->send_unit);
            menu_screen_thread_request_redraw(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL);
            return;
        }
    }

    if (s_proposition_send_unit_count == 3) {
        g_display_object_animated_pool[level->send_unit.frame_window_a].palette
            = g_display_object_animated_pool[level->send_unit.frame_window_b].palette = 0xA;
        g_display_object_gs_image_pool[level->send_unit.portrait_frame].flags |= DISPLAY_OBJECT_POP_IN;
        g_display_object_gs_image_pool[level->send_unit.portrait_frame].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_menu_blocked_buttons = -1;
        g_display_object_animated_pool[level->send_unit.portrait_window].flags ^= DISPLAY_OBJECT_HIDDEN;
        level->send_unit.shown_unit = -1;
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_REPLACE;
        level->send_unit.slot = 0;
        g_display_object_animated_pool[level->send_unit.cursor_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->send_unit.cursor_window].position.x
            = g_display_object_gs_image_pool[level->send_unit.list_frame.render_index].position.x + 6;
        g_display_object_animated_pool[level->send_unit.cursor_window].position.y
            = g_display_object_gs_image_pool[level->send_unit.list_frame.render_index].position.y
            + level->send_unit.slot * 16 + 0xC;
        return;
    }

    s_proposition_send_units[s_proposition_send_unit_count] = value;
    s_proposition_send_unit_count++;

    draw_participant_names(&level->send_unit);
    build_candidates(&level->send_unit);
    menu_screen_thread_request_redraw(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL);
}
