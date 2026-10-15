#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Appends one entry to the panel list: its value column (g_wldcore_list_entry_values), a
 * cleared flag column (g_wldcore_list_row_flags), and the running count. The same
 * three-step append appears in wldcore_proposition_load_send_unit_candidates.
 *
 * The do/while(0) statement-macro wrapper is load-bearing: without it the
 * entry index and the two strength-reduced array pointers rotate through
 * $v1/$a2/$a0 instead of the target's $a2/$a0/$v1. The likely mechanism is
 * the wrapper's loop notes changing how loop.c treats the enclosing loop. */
#define append_panel_list_entry(count, value)                                                                          \
    do {                                                                                                               \
        g_wldcore_list_entry_values[count] = value;                                                                    \
        g_wldcore_list_row_flags[count] = 0;                                                                           \
        count = (count) + 1;                                                                                           \
    } while (0)

/* Opens a 41-entry panel list (16-bit values 0x8000-0x8028), shows up to
 * eight rows at (0x54, 0x50) with width 0x8c, and appends its 0x5c-byte
 * record tagged 0x21.
 *
 * The list is indexed by `count` rather than `i`; indexing by `i` lets loop.c
 * fold the final count to the constant 41. */
void wldcore_list_open_tutorial_topics(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 count;
    s32 i;
    s32 visible;
    s32 record_index;

    count = 0;
    for (i = 0; i < 41; i++) {
        append_panel_list_entry(count, i + (s16)TEXT_ID_TIP_NAME_BASE);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        visible = count;
    } else {
        visible = 8;
    }
    layout.text_width = 0x8c;
    origin.x = 0x54;
    layout.visible_rows = visible;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS, count, visible);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_TOPICS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

void wldcore_list_tutorial_topics_restore_blocked_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_tutorial_topics_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Tutorial-topic list level input step (cursor slot 12); twin of
 * wldcore_list_handle_treasures_input with help message 0x108d. A confirmed topic's value is
 * passed to 0x80084578, which opens the tutorial text window. */
void wldcore_list_handle_tutorial_topics_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS);
            wldcore_menu_push_tutorial_topic_text_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_tutorial_topics_block_all_buttons(level);
        wldcore_menu_push_message_level(0x108D, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes the tutorial topic text window as menu level 0x22: records the topic
 * id on the new level, sets up the shared scrollable text window at g_wldcore_scrollable_text_window
 * for that topic's text (0x8800 + topic), and hides the parent pair-render
 * level's two windows and its render record with flag 0x10. wldcore_menu_wait_tutorial_topic_text_then_pop_level
 * steps the window and pops the level, restoring all three.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The menu-stack type store comes last so the
 * compiler scales depth by four late and in place, forcing the copy into v0
 * for depth + 1. */
void wldcore_menu_push_tutorial_topic_text_level(s32 topic) {
    s32 coordinate_mode = 1;
    s32 depth;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = topic;
    g_wldcore_scrollable_text_window.text_id = topic + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xdc;
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
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_types_next[depth] = WLDCORE_MENU_LEVEL_TUTORIAL_TOPIC_TEXT;
}

/* Tutorial topic text window set up by 0x80084578. */

/* Menu level 0x22 pushed by 0x80084578: steps the tutorial text window
 * until 0x800903e4 reports it finished, then pops the level, restores the
 * parent level's windows and render record, and reopens the topic list. */
void wldcore_menu_wait_tutorial_topic_text_then_pop_level(void) {
    s32 depth;

    if (!(g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
            & WORLD_RENDER_RECORD_POP_IN)
        && wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_menu_stack_depth = depth - 1;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        wldcore_list_open_tutorial_topics();
    }
}

/* Push list-window menu level 0x25.
 *
 * Entry 0 is listed when g_main_save_proposition_count is nonzero and entry 1
 * when any script variable 0x360-0x3bf has bit 2 set; entries draw text
 * 0xb864 + entry. The cursor is restored from g_wldcore_proposition_category_saved_cursor (reset when out of
 * range). `value` holds the cursor and then the reloaded main-window index:
 * as one multi-set variable it is left to global allocation, which gives the
 * target's v1/v0 assignment in the tail. `unused` reproduces the 0x58 frame.
 */
void wldcore_menu_push_proposition_category_level(void) {
    point32_t point;
    RECT bounds;
    s32 unused[4];
    point32_t* base;
    s32 index;
    s32 i;
    s32 completed;
    s32 depth;
    s32 value;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 0;
    completed = g_main_save_proposition_count;
    if (completed != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0]
            = WLDCORE_PROPOSITION_CATEGORY_ROW_ACTIVE;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }
    completed = 0;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & WLDCORE_PROPOSITION_VARIABLE_COMPLETED) {
            completed++;
        }
    }
    if (completed != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_PROPOSITION_CATEGORY_ROW_BRAVE_STORY;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = index;
    bounds.x = 0x80;
    bounds.y = 0x80;
    bounds.w = 0x4C;
    bounds.h = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count; i++) {
        window_display_text_entry_variant(index,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[i]
                + WLDCORE_TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.main_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.side_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.upper_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.lower_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    if (g_wldcore_menu_stack_records_next[depth].list_window.entry_count
        < g_wldcore_proposition_category_saved_cursor[0] + 1) {
        g_wldcore_proposition_category_saved_cursor[0] = 0;
    }
    value = g_wldcore_proposition_category_saved_cursor[0];
    g_wldcore_menu_stack_records_next[depth].list_window.selected_entry = value;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_menu_stack_depth = depth + 1;
    value = g_wldcore_menu_stack_records_next[depth].list_window.main_window;
    g_wldcore_window_records[value].position.y = g_wldcore_window_render_records[index].base.y
        - -(g_wldcore_proposition_category_saved_cursor[0] * 0x10) + 0xE;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY;
}
