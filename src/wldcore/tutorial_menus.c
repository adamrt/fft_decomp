/* The tutorial menu opens the Formation and Battle topic lists and records which entries have been visited. */

#include "fft/bits.h"
#include "fft/brave_story.h"
#include "fft/display_object.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Two 32-bit masks, one bit per tutorial-list entry, set when the entry is chosen. */
extern u32 g_tutorial_progress_masks[];

static s32 g_tutorial_battle_event_ids[TUTORIAL_BATTLE_COUNT] = { EVENT_ID_TUTORIAL_FIRST, EVENT_ID_TUTORIAL_FIRST + 2,
    EVENT_ID_TUTORIAL_FIRST + 4, EVENT_ID_TUTORIAL_FIRST + 6, EVENT_ID_TUTORIAL_FIRST + 8, EVENT_ID_TUTORIAL_FIRST + 10,
    EVENT_ID_TUTORIAL_FIRST + 12, EVENT_ID_TUTORIAL_FIRST + 14 };

/* Pushes the tutorial category menu (MENU_STACK_LEVEL_TUTORIAL)
 * with the cursor on selected_entry.
 *
 * Appends a cursor window, a header window and the frame render record, seeds
 * the four category entries, and positions both windows relative to the render
 * record's origin. With `reset` set it also clears the two 32-bit tutorial
 * progress masks at g_tutorial_progress_masks.
 *
 * The 40 bytes of `dead_locals` reproduce the target's 0x50-byte frame;
 * the original local declarations are unknown.
 * The `- -(...)` in the cursor-row Y expression only flips the commutative
 * operand order of the final `addu`; it is arithmetically identical. */
void tutorial_menu_open(s32 selected_entry, s32 reset) {
    s32 dead_locals[10];
    s32 window_index;
    s32 record_index;

    if (reset != 0) {
        g_tutorial_progress_masks[1] = 0;
        g_tutorial_progress_masks[0] = 0;
    }

    window_index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.cursor_window = window_index;
    g_display_object_animated_pool[window_index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[window_index].priority = 0xA;
    g_display_object_animated_pool[window_index].anim_counter = 0;
    g_display_object_animated_pool[window_index].frame_index = 0;

    window_index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.label_window = window_index;
    g_display_object_animated_pool[window_index].sequence = DISPLAY_OBJECT_ANIM_LIST_SIDE_LABEL;
    g_display_object_animated_pool[window_index].priority = 0xA;
    g_display_object_animated_pool[window_index].anim_counter = 0;
    g_display_object_animated_pool[window_index].frame_index = 0;
    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.entries[0] = TUTORIAL_MENU_ROW_FORMATION;
    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.entries[1] = TUTORIAL_MENU_ROW_BATTLE;
    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.entries[2] = TUTORIAL_MENU_ROW_TOPICS;
    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.entries[3] = TUTORIAL_MENU_ROW_LEAVE;
    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.entry_count = TUTORIAL_MENU_ROW_COUNT;

    g_menu_stack_records[g_menu_stack_depth].tutorial_menu.rows_image
        = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    tutorial_menu_panel_build(&g_menu_stack_records[g_menu_stack_depth].tutorial_menu);

    record_index = g_menu_stack_depth;
    g_menu_stack_records[record_index].tutorial_menu.selected_entry = selected_entry;
    g_menu_stack_records[record_index].tutorial_menu.mode = MENU_STACK_WINDOW_MODE_BROWSE;
    g_display_object_animated_pool[g_menu_stack_records[record_index].tutorial_menu.cursor_window].position.x
        = g_display_object_gs_image_pool[g_menu_stack_records[record_index].tutorial_menu.rows_image].position.x + 6;
    g_display_object_animated_pool[g_menu_stack_records[record_index].tutorial_menu.cursor_window].position.y
        = g_display_object_gs_image_pool[g_menu_stack_records[record_index].tutorial_menu.rows_image].position.y
        - -(g_menu_stack_records[record_index].tutorial_menu.selected_entry * 0x10) + 0xE;
    g_display_object_animated_pool[g_menu_stack_records[record_index].tutorial_menu.label_window].position.x
        = g_display_object_gs_image_pool[g_menu_stack_records[record_index].tutorial_menu.rows_image].position.x + 3;
    g_menu_stack_depth = record_index + 1;
    g_display_object_animated_pool[g_menu_stack_records[record_index].tutorial_menu.label_window].position.y
        = g_display_object_gs_image_pool[g_menu_stack_records[record_index].tutorial_menu.rows_image].position.y - 2;
    g_menu_stack_records[record_index].tutorial_menu._unknown_0c = 0x18;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_TUTORIAL;
}

/* Build the image of the 0x44-wide tutorial category panel: one 16-pixel row
 * per row id (TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST + id), at
 * (-0x78, -0x28) with OT priority 0xa, popping in.
 *
 * The base coordinates are written through a point view of base_x/base_y; the
 * target addresses them from &base_x, not from the record start. */
void tutorial_menu_panel_build(tutorial_menu_level_t* panel) {
    RECT bounds;
    point32_t point;
    s32 index = panel->rows_image;
    point32_t* base;
    s32 i;

    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x44;
    bounds.h = panel->entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 0xA;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_display_object_gs_image_pool[index].position;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < panel->entry_count; i++) {
        window_display_text_entry(
            index, panel->entries[i] + TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST, point, g_window_image_buffer);
        point.y += 0x10;
    }
    window_load_render_record_image(index, g_window_image_buffer);
}

/* Restore the tutorial list's palettes and cursor animation, then show its welcome text. */
void tutorial_menu_resume(menu_stack_record_t* level) {
    s32 main_window;
    s32 side_window;
    s32 slot;
    /* The original frame reserves eight unused bytes. */
    s32 pad[2];

    main_window = level->tutorial_menu.cursor_window;
    side_window = level->tutorial_menu.label_window;
    g_display_object_gs_image_pool[level->tutorial_menu.rows_image].palette = 0;
    g_display_object_animated_pool[side_window].palette = 0;
    g_display_object_animated_pool[main_window].palette = 0;
    slot = level->tutorial_menu.cursor_window;
    g_display_object_animated_pool[slot].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    slot = level->tutorial_menu.cursor_window;
    g_display_object_animated_pool[slot].anim_counter = 0;
    g_display_object_animated_pool[slot].frame_index = 0;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
}

/* Gray out the tutorial category menu while a sub-level opens over it. The
 * target contains a second copy of bar_menu_suspend; see there for
 * the matching notes. */
void tutorial_menu_suspend(menu_stack_record_t* level) {
    s32 index;

    g_display_object_animated_pool[level->tutorial_menu.cursor_window].palette = 10;
    g_display_object_animated_pool[level->tutorial_menu.label_window].palette = 6;
    g_display_object_gs_image_pool[level->tutorial_menu.rows_image].palette = 2;
    g_display_object_animated_pool[level->tutorial_menu.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
    index = level->tutorial_menu.cursor_window;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
}

/* Input step of the tutorial category menu: Circle opens the formation tutorials, the battle
 * tutorials or the topics (or leaves); Cross or Leave shows the parting prompt and fades out, back
 * to the title when opened from there; Select shows the row's help; Up and Down wrap. */
void tutorial_menu_update(menu_stack_record_t* level) {
    u32 buttons;
    s32* selected_entry;

    if (g_display_object_gs_image_pool[level->tutorial_menu.rows_image].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) {
        if (level->tutorial_menu.mode == MENU_STACK_WINDOW_MODE_FADE_OR_HELP
            && !(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            level->tutorial_menu.mode = MENU_STACK_WINDOW_MODE_LEAVE_WAIT;
            world_map_image_buffers_clear(1);
        }
        if (level->tutorial_menu.mode == MENU_STACK_WINDOW_MODE_LEAVE_WAIT
            && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            if ((g_world_map_flags & WORLD_MAP_FLAG_TITLE_TUTORIAL) == 0 || g_music_queue.count == 0) {
                g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
                display_object_queues_set_enabled(1);
                g_menu_stack_depth = g_menu_stack_depth - 1;
                g_world_map_flags = g_world_map_flags
                    & ~(WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE | WORLD_MAP_FLAG_HIDE_CALENDAR_DATE
                        | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL);
                menu_stack_dispatch_suspend();
                if (g_world_map_flags & WORLD_MAP_FLAG_TITLE_TUTORIAL) {
                    g_world_map_flags = (g_world_map_flags ^ (WORLD_MAP_FLAG_RUNNING | WORLD_MAP_FLAG_TITLE_TUTORIAL))
                        | WORLD_MAP_FLAG_RETURN_TO_TITLE;
                    return;
                }
                world_map_fade_start(WORLD_MAP_FADE_REDRAW_MAP, 0x10);
                world_map_menu_level_open();
            }
        }
        return;
    }

    if (level->tutorial_menu.mode == MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE) {
        if (g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            level->tutorial_menu.mode = MENU_STACK_WINDOW_MODE_FADE_OR_HELP;
            thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
            g_world_map_flags = g_world_map_flags | WORLD_MAP_FLAG_INPUT_BLOCKED;
            world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
            if (g_world_map_flags & WORLD_MAP_FLAG_TITLE_TUTORIAL) {
                music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x10);
                music_queue_add(MUSIC_QUEUE_COMMAND_UNLOAD, 0);
            } else {
                music_queue_track_start(MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP));
            }
            g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_TUTORIAL].selected_index = 0;
        }
        return;
    }

    if (g_music_queue.count != 0) {
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_PROMPT, 0);
        level->tutorial_menu.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
        g_display_object_animated_count -= 2;
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 3;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        tutorial_menu_suspend(level);
        menu_stack_message_open(level->tutorial_menu.entries[level->tutorial_menu.selected_entry] + 0x108B, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        selected_entry = &level->tutorial_menu.selected_entry;
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_TUTORIAL].selected_index = *selected_entry;
        switch (level->tutorial_menu.entries[*selected_entry]) {
        case TUTORIAL_MENU_ROW_FORMATION:
            tutorial_menu_suspend(level);
            g_display_object_animated_pool[level->tutorial_menu.cursor_window].sequence
                = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
            tutorial_formation_list_open();
            return;
        case TUTORIAL_MENU_ROW_BATTLE:
            tutorial_menu_suspend(level);
            g_display_object_animated_pool[level->tutorial_menu.cursor_window].sequence
                = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
            tutorial_battle_list_open();
            return;
        case TUTORIAL_MENU_ROW_TOPICS:
            message_block_load_if_changed(MESSAGE_BLOCK_TUTORIAL_TOPICS);
            tutorial_menu_suspend(level);
            g_display_object_animated_pool[level->tutorial_menu.cursor_window].sequence
                = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
            tutorial_topic_list_open();
            return;
        case TUTORIAL_MENU_ROW_LEAVE:
            thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_PROMPT, 0);
            level->tutorial_menu.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
            g_display_object_animated_count -= 2;
            g_display_object_image_count -= 1;
            g_display_object_queue_count -= 3;
            return;
        }
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (level->tutorial_menu.selected_entry != 0) {
            g_display_object_animated_pool[level->tutorial_menu.cursor_window].position.y -= 0x10;
            level->tutorial_menu.selected_entry--;
        } else {
            g_display_object_animated_pool[level->tutorial_menu.cursor_window].position.y
                += (level->tutorial_menu.entry_count - 1) * 0x10;
            level->tutorial_menu.selected_entry = level->tutorial_menu.entry_count - 1;
        }
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        if (level->tutorial_menu.selected_entry < level->tutorial_menu.entry_count - 1) {
            g_display_object_animated_pool[level->tutorial_menu.cursor_window].position.y += 0x10;
            level->tutorial_menu.selected_entry++;
        } else {
            g_display_object_animated_pool[level->tutorial_menu.cursor_window].position.y
                -= (level->tutorial_menu.entry_count - 1) * 0x10;
            level->tutorial_menu.selected_entry = 0;
        }
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }
}

/* Opens the list of the seven formation tutorials (TEXT_ID_TUTORIAL_NAME_BASE
 * + row), a row drawn in text color 8 once its bit of
 * g_tutorial_progress_masks[0] shows it was viewed, at (0x54, 0x50),
 * 0x8c wide, and pushes MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST.
 *
 * The viewed-entry palette lives in its own variable so the target keeps 8 in a saved
 * register across the loop. */
void tutorial_formation_list_open(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 i = 0;
    s32 count;
    s32 record_index;
    s32 viewed_color = 8;

    for (; i < TUTORIAL_FORMATION_COUNT; i++) {
        g_menu_stack_entry_values[i] = i + TEXT_ID_TUTORIAL_NAME_BASE;
        if (bits_test(g_tutorial_progress_masks, i) != 0) {
            g_menu_stack_row_text_colors[i] = viewed_color;
        } else {
            g_menu_stack_row_text_colors[i] = 0;
        }
    }
    count = i;
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = i;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x54;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_FORMATION_TUTORIALS, count, i);
    window_panel_thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_menu_stack_records[record_index].panel.result = 0;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the formation tutorials list: block Circle, Cross and Select from the WORLD
 * menu frame again. */
void tutorial_formation_list_buttons_restore(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void tutorial_block_formation_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the formation tutorials list (help message 0x108b). Circle
 * fades the screen out and closes the panel; once it has closed the level
 * moves to result 2, turns the display off and closes the message box. When
 * that has ended, the tutorial is marked viewed in
 * g_tutorial_progress_masks[0], the menu brightness is restored, WORLD's
 * formation screen runs the tutorial, and a screen transition brings the
 * list back. */
void tutorial_formation_list_input_update(menu_stack_record_t* level) {
    if (level->panel.result == 2) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            s16* selected = &g_window_panel_render_state.selected_index;

            bits_value_set(g_tutorial_progress_masks, *selected, 1);
            screen_brightness_set(0x80, 0x80, 0x80);
            formation_tutorial_run(*selected);
            g_menu_stack_ot_offset = 1;
            g_menu_blocked_buttons = 0;
            g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED | WORLD_MAP_FLAG_DISPLAY_OFF;
            g_menu_stack_depth--;
            screen_transition_open(MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST, 0, 0);
        }
        return;
    }

    if (g_world_map_flags & WORLD_MAP_FLAG_FADING) {
        return;
    }

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED)
        && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        if (level->panel.result != 0) {
            menu_stack_cursor_store(MENU_STACK_CURSOR_SLOT_FORMATION_TUTORIALS);
            level->panel.result = 2;
            g_world_map_flags |= WORLD_MAP_FLAG_DISPLAY_OFF;
            thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
            return;
        }
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        menu_stack_cursor_clear(MENU_STACK_CURSOR_SLOT_FORMATION_TUTORIALS);
        menu_stack_dispatch_resume();
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        tutorial_block_formation_list_buttons(level);
        menu_stack_message_open(0x108B, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
    }
}

/* Opens the list of the eight battle tutorials (TEXT_ID_TUTORIAL_NAME_BASE +
 * 16 + row), a row drawn in text color 8 once its bit of
 * g_tutorial_progress_masks[1] shows it was viewed, at (0x54, 0x50),
 * 0x8c wide, and pushes MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST. Sibling of
 * tutorial_formation_list_open. */
void tutorial_battle_list_open(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 i;
    s32 count;
    s32 record_index;

    for (i = 0; i < TUTORIAL_BATTLE_COUNT; i++) {
        g_menu_stack_entry_values[i] = i + TEXT_ID_TUTORIAL_NAME_BASE + 16;
        if (bits_test(&g_tutorial_progress_masks[1], i) != 0) {
            g_menu_stack_row_text_colors[i] = 8;
        } else {
            g_menu_stack_row_text_colors[i] = 0;
        }
    }
    count = i;
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = i;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x54;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS, count, i);
    window_panel_thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_menu_stack_records[record_index].panel.result = 0;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the battle tutorial list: block Circle, Cross and Select from the WORLD menu
 * frame again. */
void tutorial_battle_list_buttons_restore(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withhold every button from the WORLD menu frame while a level opened from the battle tutorial
 * list runs. */
void tutorial_block_battle_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the battle tutorials list, which plays the chosen tutorial as
 * an event.
 *
 * Cross and Circle close the panel and record the choice; Circle also fades
 * the screen out, and Select on a non-empty list opens help message 0x108c.
 * Once input is blocked and the fade is over, a cancel pops the level and a
 * choice turns the display off, closes the message box and moves to result 2.
 * When the message box has gone, the tutorial is marked viewed in
 * g_tutorial_progress_masks[1], its event is set as
 * EVENT_SCRIPT_VAR_CURRENT_EVENT (with progress effects suppressed), the
 * list cursor and weather are saved, and the world loop ends with
 * WORLD_MAP_FLAG_TUTORIAL_EVENT. */
void tutorial_battle_list_input_update(menu_stack_record_t* level) {
    s16* selected;

    if (level->panel.result == 2) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            selected = &g_window_panel_render_state.selected_index;
            bits_value_set(&g_tutorial_progress_masks[1], *selected, 1);
            script_variable_set(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
            script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_tutorial_battle_event_ids[*selected]);
            menu_stack_cursor_store(MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS);
            g_brave_story_saved_list_cursor_state = g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS];
            g_world_map_flags = (g_world_map_flags | WORLD_MAP_FLAG_TUTORIAL_EVENT) ^ WORLD_MAP_FLAG_RUNNING;
            g_brave_story_saved_weather_variable = script_variable_get(EVENT_SCRIPT_VAR_WEATHER);
        }
        return;
    }

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
        if (level->panel.result != 0) {
            g_world_map_flags |= WORLD_MAP_FLAG_DISPLAY_OFF;
            thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
            level->panel.result = 2;
            return;
        }
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        menu_stack_cursor_clear(MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS);
        menu_stack_dispatch_resume();
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        tutorial_block_battle_list_buttons(level);
        menu_stack_message_open(0x108C, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
    }
}
