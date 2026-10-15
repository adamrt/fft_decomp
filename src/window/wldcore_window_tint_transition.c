#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Ends the window tint transition and restores the parent content record. */
void wldcore_window_end_tint_transition(wldcore_menu_list_window_level_t* level) {
    g_wldcore_window_records[level->main_window].palette = g_wldcore_window_render_records[level->side_window].palette
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                              .list_window.content_render]
              .palette = 0;
    g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[level->main_window].frame_index = g_wldcore_window_records[level->main_window].anim_counter
        = 0;
    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .flags
        & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }
}

/* Activates the proposition-category cursor and render record: the window takes
 * palette 10 and its render record palette 2, the window's three 32-bit scratch fields are
 * cleared, and the current menu level's list content render record is hidden
 * while the transition runs. */
void wldcore_window_begin_tint_transition(wldcore_menu_list_window_level_t* level) {
    g_wldcore_window_records[level->main_window].palette = 10;
    g_wldcore_window_render_records[level->side_window].palette = 2;
    g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    g_wldcore_window_records[level->main_window].frame_index = g_wldcore_window_records[level->main_window].anim_counter
        = 0;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                        .list_window.content_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
}

/* Input step for the brave-story top-level list window.
 *
 * Refreshes the displayed play-time copies each frame, then, while the side
 * render record is not pending (flag 0x100): button 0x100 opens the help
 * message for the highlighted row (0x105a + entry), button 0x40 pops the
 * level, button 0x20 dispatches on the row value (0 opens the brave-story
 * submenu at wldcore_list_open_active_propositions, 1 the proposition list), and up/down wrap the
 * cursor, moving the cursor window by 16 pixels a row. The wrap-around move
 * is spelled as a subtraction of a negative step to keep the window's y as
 * the first addition operand. */
void wldcore_menu_handle_proposition_category_input(wldcore_menu_list_window_level_t* level) {
    s32 window;
    s32 entry;
    s32 count;
    u32 buttons;

    g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
    g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
    g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;

    if ((g_wldcore_window_render_records[level->side_window].flags & WORLD_RENDER_RECORD_POP_IN) != 0) {
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_wldcore_window_records[level->main_window].palette = 0xA;
        g_wldcore_window_render_records[level->side_window].palette = 2;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .palette = 2;
        g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        window = level->main_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        wldcore_menu_push_message_level(level->entries[level->selected_entry] + 0x105A, 1);
        return;
    }
    if ((buttons & PSX_PAD_CROSS) != 0 && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_proposition_category_saved_cursor[0] = 0;
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) != 0 && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_proposition_category_saved_cursor[0] = level->selected_entry;
        entry = level->entries[level->selected_entry];
        switch (entry) {
        case WLDCORE_PROPOSITION_CATEGORY_ROW_ACTIVE:
            wldcore_window_begin_tint_transition(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_active_propositions();
            break;
        case WLDCORE_PROPOSITION_CATEGORY_ROW_BRAVE_STORY:
            wldcore_window_begin_tint_transition(level);
            g_wldcore_window_records[level->main_window].sequence = entry;
            wldcore_list_open_brave_story_propositions();
            break;
        }
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->entry_count >= 2) {
            if (level->selected_entry != 0) {
                g_wldcore_window_records[level->main_window].position.y -= 0x10;
                level->selected_entry = level->selected_entry - 1;
            } else {
                g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * -0x10;
                level->selected_entry = level->entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        count = level->entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->selected_entry < count) {
                g_wldcore_window_records[level->main_window].position.y += 0x10;
                level->selected_entry = level->selected_entry + 1;
            } else {
                g_wldcore_window_records[level->main_window].position.y -= count * 0x10;
                level->selected_entry = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}

/* Pushes the active-proposition list level (type 0x26).
 *
 * Starts WORLD thread 14 on entry point 0xb848, re-shows the parent level's
 * cursor window and list panel (popping the panel in) when the panel is
 * hidden, then appends a cursor window and
 * three column-header windows (sequences 2 / 0x63 / 0x64 / 0x65) plus one
 * render record sized to g_main_save_proposition_count rows.  One row is
 * seeded per saved proposition, and the panel image is drawn in three passes:
 * the proposition name at x=8 (text 0xd000 | proposition_id), the participant
 * count at x=0x9c (text 0xb866) and the days remaining at x=0xb6
 * (text 0xb867, assigned_days - elapsed_days); both numeric columns go
 * through g_text_substitution_values[0].  Finally the saved cursor row
 * in g_wldcore_active_propositions_saved_cursor is clamped, the four windows are positioned relative to the
 * render record's origin, and the level is committed at depth + 1.
 *
 * The 0xb8 frame carries 0x50 bytes of locals the original never reads;
 * `dead_locals` reproduces them.
 *
 * The seeding loop's entry test is spelled `i < count` (the target's signed
 * `blez`) around a do-while that re-reads the u8 count, which gives the
 * hoisted bound copy the target keeps in $a0.  The cursor row is folded as
 * base_y - row * -16 + 14 to keep base_y as the first addu operand.
 */
void wldcore_list_open_active_propositions(void) {
    point32_t point;
    RECT bounds;
    point32_t* base;
    s32 index;
    s32 depth;
    s32 i;
    s32 temp;
    s32 dead_locals[20];

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);

    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .proposition_list.title_window]
            .flags
        & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .proposition_list.cursor_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .proposition_list.title_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .proposition_list.title_window]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    i = 0;
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.title_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_TITLE_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.count_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.days_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_DAYS_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count = 0;
    while (i < g_main_save_proposition_count) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .proposition_list
            .entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count] = i;
        i++;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count++;
    }

    i = 0;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.render_index = index;
    bounds.y = 0x40;
    bounds.x = 0;
    bounds.w = 0xD8;
    bounds.h = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 2, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x60;
    base->y = -0x28;

    point.x = 8;
    point.y = 8;
    while (i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count) {
        window_display_text_entry_variant(index,
            g_main_active_propositions[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                           .proposition_list.entries[i]]
                    .proposition_id
                | TEXT_ID_PROPOSITION_NAME_BASE,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0x9C;
    point.y = 8;
    i = 0;
    while (i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count) {
        g_text_substitution_values[0]
            = g_main_active_propositions[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                             .proposition_list.entries[i]]
                  .participant_count;
        window_display_text_entry_variant(index, WLDCORE_TEXT_ID_VALUE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0xB6;
    point.y = 8;
    i = 0;
    while (i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count) {
        s32 entry = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entries[i];
        g_text_substitution_values[0]
            = g_main_active_propositions[entry].assigned_days - g_main_active_propositions[entry].elapsed_days;
        window_display_text_entry_variant(index, WLDCORE_TEXT_ID_TWO_DIGIT_VALUE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    depth = g_wldcore_menu_stack_depth;
    /* temp spans two blocks without crossing a call, so global allocation
     * gives the clamp flag and the cursor copy the target's v1. */
    temp = g_wldcore_menu_stack_records_next[depth].proposition_list.entry_count
        < g_wldcore_active_propositions_saved_cursor[0] + 1;
    if (temp) {
        g_wldcore_active_propositions_saved_cursor[0] = 0;
    }
    temp = g_wldcore_active_propositions_saved_cursor[0];
    g_wldcore_menu_stack_records_next[depth].proposition_list.cursor = temp;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y - g_wldcore_active_propositions_saved_cursor[0] * -0x10 + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.title_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.title_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.count_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0x94;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.count_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.days_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0xB2;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.days_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_ACTIVE_PROPOSITIONS;
}
