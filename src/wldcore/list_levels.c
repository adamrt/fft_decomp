/* List levels, including tint transitions, proposition categories and active propositions, participant lists and
 * details, Brave Story propositions and attempt details, screen transitions, the window publish slot, and the tutorial
 * category, formation and mask 1 lists. */

#include "fft/data.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_text.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void wldcore_list_build_tutorial_category_panel_image(wldcore_text_list_panel_t* panel);
static void wldcore_list_open_active_propositions(void);
static void wldcore_list_open_brave_story_propositions(void);
static void wldcore_list_open_formation_tutorials(void);
static void wldcore_list_open_tutorial_categories(s32 selected_entry, s32 reset);
static void wldcore_list_open_battle_tutorials(void);
static void wldcore_menu_push_participant_detail_level(s32 proposition, s32 participant);
static void wldcore_menu_push_participant_list_level(s32 proposition);
static void wldcore_menu_push_proposition_attempt_detail_level(s32 value);
static void wldcore_window_close_parent_list_all_windows(void);
static void wldcore_window_close_parent_list_frame_windows(void);

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
static void wldcore_list_open_active_propositions(void) {
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

/* Resume handler of the active-proposition list: restore the palettes of its four windows and
 * render record and idle the cursor, undoing wldcore_window_set_layout_states. */
void wldcore_window_clear_layout_states(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_records[level->proposition_list.cursor_window].palette
        = g_wldcore_window_records[level->proposition_list.title_window].palette
        = g_wldcore_window_records[level->proposition_list.count_window].palette
        = g_wldcore_window_records[level->proposition_list.days_window].palette
        = g_wldcore_window_render_records[level->proposition_list.render_index].palette = 0;
    g_wldcore_window_records[level->proposition_list.cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[level->proposition_list.cursor_window].frame_index
        = g_wldcore_window_records[level->proposition_list.cursor_window].anim_counter = 0;
}

/* Select the palettes for the active-proposition list and its four windows. */
void wldcore_window_set_layout_states(wldcore_menu_proposition_list_level_t* level) {
    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_records[level->title_window].palette = g_wldcore_window_records[level->count_window].palette
        = g_wldcore_window_records[level->days_window].palette = 6;
    g_wldcore_window_render_records[level->render_index].palette = 2;
}

/* Input handler for the proposition list level (type 0x26).
 *
 * The level's first five 32-bit values are the window/render indices that
 * wldcore_window_set_layout_states consumes. The wrap-around move is spelled as
 * a subtraction of a negative step: that keeps the window's y operand first
 * in the addition, as the target's register assignment requires. */
void wldcore_list_handle_active_propositions_input(wldcore_menu_proposition_list_level_t* level) {
    u32 buttons;
    s32 count;
    s32 window;

    if ((g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) != 0) {
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        window = level->cursor_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        wldcore_window_set_layout_states(level);
        wldcore_menu_push_message_level(0x105A, 1);
        return;
    }
    if ((buttons & PSX_PAD_CROSS) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_active_propositions_saved_cursor[0] = 0;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_active_propositions_saved_cursor[0] = level->cursor;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_push_participant_list_level(level->entries[level->cursor]);
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->entry_count >= 2) {
            if (level->cursor != 0) {
                g_wldcore_window_records[level->cursor_window].position.y -= 0x10;
                level->cursor = level->cursor - 1;
            } else {
                g_wldcore_window_records[level->cursor_window].position.y -= (level->entry_count - 1) * -0x10;
                level->cursor = level->entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        count = level->entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->cursor < count) {
                g_wldcore_window_records[level->cursor_window].position.y += 0x10;
                level->cursor = level->cursor + 1;
            } else {
                g_wldcore_window_records[level->cursor_window].position.y -= count * 0x10;
                level->cursor = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}

/* Pushes the proposition participant list level (type 0x27) for
 * g_main_active_propositions[proposition].
 *
 * The 0x40 frame needs 32 bytes of locals the body never uses. The `- -`
 * spellings keep the target's operand order (render-record base first) that a
 * plain `+` of a loaded field loses to expand_binop's commutative swap. */
static void wldcore_menu_push_participant_list_level(s32 proposition) {
    /* Target frame evidence establishes 32 bytes of otherwise unused locals. */
    u8 stack_scratch[32];
    s32 depth;
    s32 window;

    wldcore_unpack_proposition_row(
        g_wldcore_selected_proposition_row, g_main_active_propositions[proposition].proposition_id);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant_list.proposition = proposition;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_render.window_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_render.render_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    window = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant_list.cursor_window = window;
    g_wldcore_window_records[window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[window].priority = 8;
    g_wldcore_window_records[window].anim_counter = 0;
    g_wldcore_window_records[window].frame_index = 0;
    wldcore_list_build_proposition_detail_panel_image(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth], proposition, 1);
    g_text_substitution_values[0]
        = g_main_active_propositions[proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].participant_list.cursor_window].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].participant_list.render_index].base.x
        + 0x38;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].participant_list.cursor_window].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].participant_list.render_index].base.y
        - -(g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index * 16)
        - -(g_wldcore_menu_stack_records_next[depth].participant_list.panel_rows * 16 + 0xE);
    g_wldcore_menu_stack_records_next[depth].participant_list.entry_count
        = g_main_active_propositions[proposition].participant_count;
    if (g_wldcore_menu_stack_records_next[depth].participant_list.entry_count
        < g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index + 1) {
        g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index = 0;
    }
    g_wldcore_menu_stack_records_next[depth].participant_list.cursor
        = g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_records_next[depth].participant_list.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PARTICIPANT_LIST;
}

/* Input step of the proposition participant-chooser level.
 *
 * Nothing happens while the render record is hidden (flag 0x100). Once WORLD
 * thread 14's closing animation has finished, the level hands the selected
 * row to wldcore_menu_push_participant_detail_level. Cancel (0x40) tears the level down and returns to the
 * proposition list; confirm (0x20) records the row in g_wldcore_participant_list_saved_cursor and starts
 * thread 14 instead. Both drop one window record, one entry of g_wldcore_window_render_record_count and
 * two render objects. Up/down wrap the cursor and slide the cursor window by
 * one 16-pixel row, and do nothing when there are fewer than two rows.
 *
 * The two directional blocks mirror wldcore_list_handle_active_propositions_input: the up wrap is spelled as
 * a subtraction of a negative step so the window's y stays the first addend,
 * and the down block decrements its own count in place. */
void wldcore_menu_handle_participant_list_input(wldcore_menu_stack_record_t* level) {
    s32 count;

    if (g_wldcore_window_render_records[level->participant_list.render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        g_wldcore_menu_stack_depth--;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_menu_push_participant_detail_level(level->participant_list.proposition, level->participant_list.cursor);
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_participant_list_saved_cursor = 0;
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_active_propositions();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_window_record_count--;
        g_wldcore_participant_list_saved_cursor = level->participant_list.cursor;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->participant_list.entry_count >= 2) {
            if (level->participant_list.cursor != 0) {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y -= 0x10;
                level->participant_list.cursor = level->participant_list.cursor - 1;
            } else {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y
                    -= (level->participant_list.entry_count - 1) * -0x10;
                level->participant_list.cursor = level->participant_list.entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        count = level->participant_list.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->participant_list.cursor < count) {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y += 0x10;
                level->participant_list.cursor = level->participant_list.cursor + 1;
            } else {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y -= count * 0x10;
                level->participant_list.cursor = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}

/* Push the proposition participant detail level (type 0x28).
 *
 * Copies the 0x40x0x100 VRAM block at (0x180, 0) to (0x200, 0x100), appends
 * the two frame windows, draws the selected participant and restarts the
 * three core background threads. Each window has its own pos/window pointer:
 * a single-set pointer keeps its address computation beside its first use.
 */
static void wldcore_menu_push_participant_detail_level(s32 proposition, s32 participant) {
    RECT rect;
    world_animated_render_record_t* window1;
    world_animated_render_record_t* window2;
    point32_t* pos1;
    point32_t* pos2;
    s32 index;
    s32 depth;

    rect.x = 0x180;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x100;
    MoveImage(&rect, 0x200, 0x100);
    DrawSync(0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.proposition = proposition;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.participant = participant;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.left_window = index;
    g_wldcore_window_records[index].priority = 8;
    pos1 = &g_wldcore_window_records[index].position;
    pos1->x = -0x74;
    pos1->y = -0x69;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
    window1 = &g_wldcore_window_records[index];
    window1->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.right_window = index;
    g_wldcore_window_records[index].priority = 8;
    pos2 = &g_wldcore_window_records[index].position;
    pos2->x = 0x60;
    pos2->y = -0x69;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
    window2 = &g_wldcore_window_records[index];
    window2->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    wldcore_unit_build_status_panel_data(g_main_active_propositions[proposition].participant_indices[participant]);

    g_wldcore_thread8_params.x = 0;
    g_wldcore_thread8_params.y = 0;
    g_wldcore_threadc_params.y = 0;
    g_wldcore_thread9_params.y = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_wldcore_thread8_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_wldcore_threadc_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_wldcore_thread9_params, 0, 0);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].participant._unknown_08 = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Loads party member party_index into the scratch unit at 0x800d467c and
 * fills the status-panel records the participant detail level draws.
 *
 * Empty hand slots fall back to the shield; empty equipment reads as -1 and
 * a zero ability id is decremented to -1. Monsters show their primary
 * skillset's first five abilities instead of their equipped ability set. */
void wldcore_unit_build_status_panel_data(s32 party_index) {
    unit_stats_t* unit;
    menu_unit_status_record_t* gauges;
    menu_unit_status_identity_t* identity;
    menu_status_panel_slot_state_t* detail;
    u16* abilities;
    s32 i;
    u8 flags;
    u16 maximum_hp;
    u16 maximum_mp;

    if (main_unit_init_from_party_slot(&g_wldcore_status_unit, party_index, 1) == -1) {
        return;
    }
    unit = &g_wldcore_status_unit;
    gauges = &g_menu_selected_unit_stat_summary;
    identity = &g_world_selected_unit_identity;
    detail = &g_world_selected_unit_stat_detail.state;
    gauges->team_kind = 0;
    gauges->level = unit->level;
    flags = unit->team_flags;
    if (flags & BATTLE_TEAM_MASK) {
        gauges->team_kind = 1;
    }
    if (!(flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        gauges->team_kind = 2;
    }
    gauges->list_index = 0;
    gauges->unit_count = 0;
    gauges->experience = unit->experience;
    maximum_hp = unit->max_hp;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp;
    if (maximum_hp == 0) {
        gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp + 1;
    }
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    maximum_mp = unit->max_mp;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp;
    if (maximum_mp == 0) {
        gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp + 1;
    }
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->mp;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = unit->ct;
    identity->job_id = unit->job_id;
    identity->brave = unit->brave;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    gauges->unit_index = party_index;
    identity->unit_index = party_index;
    identity->roster_slot = party_index;
    identity->faith = unit->faith;
    identity->zodiac = unit->birthday.zodiac;
    detail->move = unit->move;
    detail->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    detail->jump = unit->jump;
    detail->right_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    detail->left_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    detail->right_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    detail->left_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    detail->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    detail->physical_class_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    detail->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE]) {
        detail->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    }
    detail->physical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    detail->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    detail->magical_class_evade = 0;
    detail->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE]) {
        detail->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    }
    detail->magical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    detail->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]
        = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
        ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
        : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    detail->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]
        = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
        ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
        : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    detail->equipment[MENU_EQUIPMENT_SLOT_HEAD] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
    detail->equipment[MENU_EQUIPMENT_SLOT_BODY] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
    detail->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
        if (detail->equipment[i] == 0 || detail->equipment[i] == ITEM_ID_NONE) {
            detail->equipment[i] = -1;
        }
    }
    detail->generic_monster = unit->unit_flags & UNIT_FLAG_MONSTER;
    if (detail->generic_monster != 0) {
        abilities = main_ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        for (i = 0; i < 5; i++) {
            detail->ability_ids[i] = *abilities++;
        }
    } else {
        detail->ability_ids[0] = unit->primary_skillset;
        detail->ability_ids[1] = unit->secondary_skillset;
        detail->ability_ids[2] = unit->abilities.reaction;
        detail->ability_ids[3] = unit->abilities.support;
        detail->ability_ids[4] = unit->abilities.movement;
    }
    for (i = 0; i < 5; i++) {
        if (detail->ability_ids[i] == 0) {
            detail->ability_ids[i]--;
        }
    }
}

/* Restore the participant detail windows' palettes. */
void wldcore_window_clear_pair_state(wldcore_menu_stack_record_t* level) {
    s32 second = level->participant.left_window;

    g_wldcore_window_records[level->participant.right_window].palette = 0;
    g_wldcore_window_records[second].palette = 0;
}

/* The g_wldcore_menu_publish_handlers entry of WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL: nothing to publish. */
void wldcore_menu_publish_participant_detail_level(void) {
}

/*
 * Input step of the proposition participant detail level (the unit status
 * panels of one unit sent on a job).
 *
 * Circle or Cross asks the three panel threads to close and blocks input;
 * once the status banner thread has ended the level pops, the saved screen
 * strip is moved back, line 0xB849 is shown with the proposition's name and
 * the participant list level is pushed again. Select opens the layout window.
 * R1 and L1 step to the next or previous participant with wrap-around and
 * request a redraw of the panels (banner direction 1 or 2); with a single
 * participant they only play the page sound. The L1/R1 prompts show their
 * pressed sequence while held, and one button is ignored while the other's
 * pressed sequence is showing.
 */
void wldcore_menu_handle_participant_detail_input(wldcore_menu_participant_level_t* level) {
    RECT rect;
    u32 buttons;
    s32 count;
    s32 direction;
    s32 participant;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) == 0) {
        rect.x = 0x200;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0x100;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_INPUT_BLOCKED | MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS);
        g_wldcore_menu_stack_depth--;
        MoveImage(&rect, 0x180, 0);
        DrawSync(0);
        g_text_substitution_values[0]
            = g_main_active_propositions[level->proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
        wldcore_menu_push_participant_list_level(level->proposition);
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_object_count -= 2;
        thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
            = 0xA;
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_menu_push_layout_window_level();
        return;
    }
    if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
        g_wldcore_window_records[level->left_window].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
        g_wldcore_window_records[level->right_window].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
        g_wldcore_window_records[level->left_window].frame_index
            = g_wldcore_window_records[level->left_window].anim_counter
            = g_wldcore_window_records[level->right_window].frame_index
            = g_wldcore_window_records[level->right_window].anim_counter = 0;
    }
    if (wldcore_input_update_repeat(PSX_PAD_R1) != 0
        && g_wldcore_window_records[level->left_window].sequence != WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS) {
        g_wldcore_window_records[level->right_window].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS;
        g_wldcore_window_records[level->right_window].frame_index
            = g_wldcore_window_records[level->right_window].anim_counter = 0;
        count = g_main_active_propositions[level->proposition].participant_count;
        if (count == 1) {
            /* Shared tail: both single-participant cases branch to the closing
             * sound effect; nesting the redraw under `count != 1` changes the
             * register allocation. */
            goto done;
        }
        participant = level->participant + 1;
        if (participant >= count) {
            level->participant = 0;
        } else {
            level->participant = participant;
        }
        direction = 1;
    } else {
        if (wldcore_input_update_repeat(PSX_PAD_L1) == 0
            || g_wldcore_window_records[level->right_window].sequence == WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS) {
            return;
        }
        g_wldcore_window_records[level->left_window].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS;
        g_wldcore_window_records[level->left_window].frame_index
            = g_wldcore_window_records[level->left_window].anim_counter = 0;
        if (g_main_active_propositions[level->proposition].participant_count == 1) {
            goto done;
        }
        if (level->participant != 0) {
            level->participant--;
        } else {
            level->participant = g_main_active_propositions[level->proposition].participant_count - 1;
        }
        direction = 2;
    }
    g_wldcore_thread8_params.redraw_request = direction;
    wldcore_unit_build_status_panel_data(
        g_main_active_propositions[level->proposition].participant_indices[level->participant]);
    g_wldcore_threadc_params.redraw_request = 1;
    g_wldcore_thread9_params.redraw_request = 1;
done:
    wldcore_sound_play_effect(MAIN_SFX_PAGE_SWITCH);
}

static void wldcore_list_open_brave_story_propositions(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & WLDCORE_PROPOSITION_VARIABLE_COMPLETED) {
            *values = i + TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
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
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 8;
    record_index = g_wldcore_menu_stack_depth;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index - 1].list_window.content_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_types_next[record_index] = WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* The g_wldcore_menu_resume_handlers entry of WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS: nothing to restore. */
void wldcore_menu_resume_brave_story_propositions_level(void) {
}

/* The g_wldcore_menu_publish_handlers entry of WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS: nothing to publish. */
void wldcore_menu_publish_brave_story_propositions_level(void) {
}

/* List level input step for cursor slot 16 (the list opened by 0x80087b94):
 * twin of wldcore_list_handle_unexplored_lands_input with help message 0x105b and no file-loading gate.
 * Closing without a selection re-flags the list window's content render
 * record (0x100) before returning to the bar menu; a confirmed entry goes
 * to 0x80087fa4. */
void wldcore_list_handle_brave_story_propositions_input(wldcore_menu_panel_level_t* level) {
    s32 depth;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        depth = g_wldcore_menu_stack_depth;
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth = depth - 1;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS);
            wldcore_menu_push_proposition_attempt_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.content_render].flags
            |= WORLD_RENDER_RECORD_POP_IN;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_menu_push_message_level(0x105B, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes the proposition-attempt detail text window as menu level 0x2a, the
 * partner of the 0x80088180 pop. Records the entry on the new level, decodes
 * that entry's completion date out of the packed nine-bit array at
 * g_main_proposition_last_attempt_date_bits (dest[0] month 1-12, dest[1] day
 * 1-31), and sets up the shared scrollable text window at g_wldcore_scrollable_text_window for text
 * 0x8800 + entry with the month name (0xb84f + month) and the day as its two
 * text substitutions. Then hides the parent window-render level's window and
 * render record with flag 0x10 and starts WORLD thread 14 on text 0xb849.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
static void wldcore_menu_push_proposition_attempt_detail_level(s32 value) {
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    file_wait_for_pending_variant();
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = value;
    wldcore_calendar_read_date(g_main_proposition_last_attempt_date_bits, value, &date);
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
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
    g_wldcore_scrollable_text_window.priority = 8;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = date.month + WLDCORE_TEXT_ID_MONTH_NAME_BASE;
    g_wldcore_scrollable_text_window.text_substitutions[1] = date.day;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_render.window_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_render.render_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_ATTEMPT_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Return from a message level to the window/render level beneath it.
 *
 * Waits until the message render record has left its 0x100 state and
 * wldcore_text_is_window_finished reports the message window finished, then clears flag 0x10 on
 * the parent level's window and render records, pops the stack and marks the
 * render record with 0x100 before wldcore_list_open_brave_story_propositions redraws. Mirrors
 * wldcore_menu_wait_text_then_pop_level. */
void wldcore_menu_wait_brave_story_text_then_pop_level(void) {
    s32* index = &g_wldcore_scrollable_text_window.render_record_index;
    s32 depth;

    if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_render.window_index].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_render.render_index].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth - 1;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_render.render_index].flags
        |= WORLD_RENDER_RECORD_POP_IN;
    wldcore_list_open_brave_story_propositions();
}

/* Push a screen transition into `screen` (a menu level type): clear the map, hide date, marker and
 * value, fade in, do that screen's setup (music, list, message block), clear the scratch buffer
 * and start the zoom thread; param_a and param_b are kept for the new screen. */
void wldcore_menu_push_screen_transition_level(s32 screen, s32 param_a, s32 param_b) {
    u32* clear;
    s32 count;
    s32 i;
    s32 depth;

    wldcore_clear_screen_and_set_map_clip_rect(1);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
        | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    wldcore_fade_start_screen(0, 0x10);
    switch (screen) {
    case WLDCORE_MENU_LEVEL_BRAVE_STORY:
        wldcore_window_set_render_objects_visible(0);
        wldcore_sound_wait_for_queue_drain();
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BRAVE_STORY));
        break;
    case WLDCORE_MENU_LEVEL_STORY_EVENTS:
        wldcore_window_set_render_objects_visible(0);
        world_menu_set_slot_4_cursor_index(2);
        count = 0;
        wldcore_sound_wait_for_queue_drain();
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BRAVE_STORY));
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        if (script_get_variable(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
            for (i = 0; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
                if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
                    count++;
                }
            }
        }
        if (count != 0) {
            g_wldcore_brave_story_saved_cursor[0] = 1;
        } else {
            g_wldcore_brave_story_saved_cursor[0] = 0;
        }
        wldcore_menu_push_brave_story_level();
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        wldcore_window_close_parent_list_all_windows();
        wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_STORY_EVENTS);
        file_wait_for_pending_variant();
        g_wldcore_story_events_cursor_state = g_main_saved_list_cursor_state;
        break;
    case WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES:
        wldcore_window_set_render_objects_visible(0);
        wldcore_sound_wait_for_queue_drain();
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_TUTORIAL));
        break;
    case WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS:
        wldcore_list_build_tutorial_category_panel_image(
            &g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].text_list);
        wldcore_window_close_parent_list_frame_windows();
        break;
    case WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES:
        wldcore_window_set_render_objects_visible(0);
        world_menu_set_slot_4_cursor_index(3);
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        wldcore_list_open_tutorial_categories(1, 0);
        wldcore_window_close_parent_list_frame_windows();
        g_wldcore_tutorial_entries_cursor_state = g_main_saved_list_cursor_state;
        break;
    case WLDCORE_MENU_LEVEL_TREASURE_DETAIL:
        wldcore_window_close_parent_list_all_windows();
        wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_TREASURES);
        file_wait_for_pending_variant();
        wldcore_sound_wait_for_queue_drain();
        suzuki_sound_set_master_volume(0x3FFF, 1);
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BRAVE_STORY));
        break;
    default:
        break;
    }
    i = 0x77FF;
    clear = &g_wldcore_scratch_buffer[0x77FF];
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].screen_transition.screen = screen;
    g_wldcore_menu_stack_records_next[depth].screen_transition.param_a = param_a;
    g_wldcore_menu_stack_records_next[depth].screen_transition.param_b = param_b;
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }
    world_menu_set_brightness(0, 0, 0);
    thread_start(WLDCORE_THREAD_SLOT_TRANSITION, world_formation_view_zoom_thread);
    thread_set_parameters(WLDCORE_THREAD_SLOT_TRANSITION, 0, 0, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].screen_transition.delay = 0x14;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].screen_transition.phase = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SCREEN_TRANSITION;
    g_wldcore_menu_stack_depth++;
}

/* Closes every window of the list-window level beneath the current one: the
 * main window goes to palette 10 (sequence, frame_index and anim_counter
 * cleared), the side window to palette 6, the frame and content render
 * records to palette 2, and the upper/lower windows to palette 5, each hidden with flag 0x10. */
static void wldcore_window_close_parent_list_all_windows(void) {
    s32 index = g_wldcore_menu_stack_depth - 1;

    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].palette = 10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].sequence
        = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].frame_index
        = g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].anim_counter = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].palette = 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].palette = 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.content_render].palette = 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.content_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.upper_window].palette = 5;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.upper_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.lower_window].palette = 5;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.lower_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
}

/* Closes the list window of the level beneath the current one: the main
 * window goes to palette 10 (sequence, frame_index and anim_counter cleared),
 * the side window to palette 6 and the frame render record to palette 2, each hidden with
 * flag 0x10. */
static void wldcore_window_close_parent_list_frame_windows(void) {
    s32 index = g_wldcore_menu_stack_depth - 1;

    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].palette = 10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].sequence
        = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].frame_index
        = g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].anim_counter = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].palette = 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].palette = 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
}

/*
 * Steps the menu level that swaps the world-map screen for a full-screen
 * menu level.
 *
 * After `delay` frames it stores the 256x240 frame buffer into the scratch
 * buffer, asks the transition thread to finish and forgets the loaded
 * background set. Once that thread has ended it pops itself, restarts the
 * message box (line 0xB848, or 0xB8DD for the tutorial lists) and pushes the
 * level named by `screen`: Brave Story, a story event text, the tutorial
 * categories, the formation tutorials, the mask-1 tutorial entries or a
 * treasure detail. The two tutorial entry lists first un-hide the parent
 * list's windows and frame.
 */
void wldcore_menu_step_screen_transition_level(wldcore_menu_screen_transition_level_t* level) {
    RECT rect;

    if (level->delay == 0) {
        if (level->phase == 0) {
            DrawSync(0);
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
            StoreImage(&rect, g_wldcore_scratch_buffer);
            DrawSync(0);
            VSync(0);
            thread_set_parameters(WLDCORE_THREAD_SLOT_TRANSITION, 0, 0, 1);
            g_wldcore_loaded_background_set = -1;
            level->phase++;
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_TRANSITION) != 0) {
            return;
        }
        g_wldcore_menu_stack_depth--;
        switch (level->screen) {
        case WLDCORE_MENU_LEVEL_BRAVE_STORY:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
            wldcore_menu_push_brave_story_level();
            break;
        case WLDCORE_MENU_LEVEL_STORY_EVENTS:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
            wldcore_menu_push_story_event_text_level(g_main_replay_story_event_index);
            break;
        case WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
            wldcore_list_open_tutorial_categories(level->param_a, level->param_b);
            break;
        case WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.main_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.side_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                .list_window.frame_render]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            wldcore_list_open_formation_tutorials();
            break;
        case WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.main_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.side_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                .list_window.frame_render]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            wldcore_list_open_battle_tutorials();
            break;
        case WLDCORE_MENU_LEVEL_TREASURE_DETAIL:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
            wldcore_menu_push_treasure_detail_level(level->param_a);
            break;
        }
    } else {
        level->delay--;
    }
}

/* Pushes the tutorial-category list level (type 0x2b).
 *
 * Appends a cursor window, a header window and the frame render record, seeds
 * the four category entries, and positions both windows relative to the render
 * record's origin. With `reset` set it also clears the two 32-bit tutorial
 * progress masks at g_main_tutorial_progress_masks.
 *
 * The 40 bytes of `dead_locals` reproduce the target's 0x50 frame: the
 * original declares locals it never reads, and GCC still reserves their slots.
 * The `- -(...)` in the cursor-row Y expression only flips the commutative
 * operand order of the final `addu`; it is arithmetically identical. */
static void wldcore_list_open_tutorial_categories(s32 selected_entry, s32 reset) {
    s32 dead_locals[10];
    s32 window_index;
    s32 record_index;

    if (reset != 0) {
        g_main_tutorial_progress_masks[1] = 0;
        g_main_tutorial_progress_masks[0] = 0;
    }

    window_index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = window_index;
    g_wldcore_window_records[window_index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[window_index].priority = 0xA;
    g_wldcore_window_records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;

    window_index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = window_index;
    g_wldcore_window_records[window_index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[window_index].priority = 0xA;
    g_wldcore_window_records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_FORMATION;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[1]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_MASK_1_ENTRIES;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[2]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_TOPICS;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[3]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_LEAVE;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 4;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.frame_render
        = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    wldcore_list_build_tutorial_category_panel_image(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].text_list);

    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].list_window.selected_entry = selected_entry;
    g_wldcore_menu_stack_records_next[record_index].list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.main_window].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.x
        + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.main_window].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.y
        - -(g_wldcore_menu_stack_records_next[record_index].list_window.selected_entry * 0x10) + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.side_window].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.x
        + 3;
    g_wldcore_menu_stack_depth = record_index + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.side_window].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.y
        - 2;
    g_wldcore_menu_stack_records_next[record_index].list_window.content_render = 0x18;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES;
}

/* Build the image of a 0x44-wide text list panel: one 16-pixel row per text id
 * (text 0xb8d9 + id), anchored at (-0x78, -0x28) in coordinate mode 0xa.
 *
 * The base coordinates are written through a point view of base_x/base_y; the
 * target addresses them from &base_x, not from the record start. */
static void wldcore_list_build_tutorial_category_panel_image(wldcore_text_list_panel_t* panel) {
    RECT bounds;
    point32_t point;
    s32 index = panel->render_index;
    point32_t* base;
    s32 i;

    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x44;
    bounds.h = panel->row_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < panel->row_count; i++) {
        window_display_text_entry_variant(index, panel->text_ids[i] + WLDCORE_TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}

/* Restore the tutorial list's palettes and cursor animation, then show its welcome text. */
void wldcore_tutorial_restore_list_and_show_welcome(wldcore_menu_stack_record_t* level) {
    s32 main_window;
    s32 side_window;
    s32 slot;
    /* The original frame reserves eight unused bytes. */
    s32 pad[2];

    main_window = level->list_window.main_window;
    side_window = level->list_window.side_window;
    g_wldcore_window_render_records[level->list_window.frame_render].palette = 0;
    g_wldcore_window_records[side_window].palette = 0;
    g_wldcore_window_records[main_window].palette = 0;
    slot = level->list_window.main_window;
    g_wldcore_window_records[slot].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    slot = level->list_window.main_window;
    g_wldcore_window_records[slot].anim_counter = 0;
    g_wldcore_window_records[slot].frame_index = 0;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
}

/* The original wrote the body of wldcore_window_publish_slot_b806 a second time as wldcore_window_publish_slot_b8dd. */
/* Sibling of the resetter at 0x8007cd78, differing only in the value stored
 * into sequence. The record stores may alias `level`, so they invalidate
 * the cached parameter loads and reproduce the target's
 * reloads; `index` shares one address computation between the last two. */
void wldcore_window_publish_slot_b8dd(wldcore_menu_list_window_level_t* level) {
    s32 index;

    g_wldcore_window_records[level->main_window].palette = 10;
    g_wldcore_window_records[level->side_window].palette = 6;
    g_wldcore_window_render_records[level->frame_render].palette = 2;
    g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
    index = level->main_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}

/* Input step of the tutorial category menu: Circle opens the formation tutorials, the mask-1
 * entries or the topics (or leaves); Cross or Leave shows the parting prompt and fades out, back
 * to the title when opened from there; Select shows the row's help; Up and Down wrap. */
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
            wldcore_list_open_battle_tutorials();
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

/* Opens the seven-row list of formation tutorials (entry values 0xc000 + row)
 * whose rows are flagged 8 when the matching bit of
 * g_main_tutorial_progress_masks is set, shown at
 * (0x54, 0x50) with width 0x8c; appends its record tagged 0x2c.
 *
 * The flag value lives in its own variable so the target keeps 8 in a saved
 * register across the loop. */
static void wldcore_list_open_formation_tutorials(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 i = 0;
    s32 count;
    s32 record_index;
    s32 enabled = 8;

    for (; i < 7; i++) {
        g_wldcore_list_entry_values[i] = i + TEXT_ID_TUTORIAL_NAME_BASE;
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

/* Resume handler of the formation tutorials list: block Circle, Cross and Select from the WORLD
 * menu frame again. */
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
static void wldcore_list_open_battle_tutorials(void) {
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

/* Resume handler of the mask-1 tutorial list: block Circle, Cross and Select from the WORLD menu
 * frame again. */
void wldcore_list_tutorial_mask_1_restore_blocked_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withhold every button from the WORLD menu frame while a level opened from the mask-1 tutorial
 * list runs. */
void wldcore_list_tutorial_mask_1_block_all_buttons(wldcore_menu_panel_level_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of a list level that hands the chosen entry to the world script.
 *
 * State 2 waits for thread 14, then marks the selected entry in tutorial
 * progress mask 1, sets script variables 0x1fc and 0x27 and requests the event
 * through system flag 0x200000. While flags & 0xc is 4, a confirmed choice
 * starts thread 14 (state 2) and a canceled one pops the level. Otherwise
 * cancel (0x40) and confirm (0x20) start thread 12 and record the choice;
 * 0x100 on a non-empty list opens help message 0x108c. */
void wldcore_list_handle_tutorial_mask_1_entries_input(wldcore_menu_panel_level_t* level) {
    s16* selected;

    if (level->result == 2) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            selected = &g_wldcore_window_panel_render_state.selected_index;
            wldcore_set_bit_value(&g_main_tutorial_progress_masks[1], *selected, 1);
            script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_tutorial_event_ids[*selected]);
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES);
            g_main_saved_list_cursor_state = g_wldcore_tutorial_entries_cursor_state;
            g_main_system_flags = (g_main_system_flags | MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) ^ MAIN_SYSTEM_FLAG_RUNNING;
            g_main_saved_weather_variable = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        }
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && !(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
        if (level->result != 0) {
            g_main_system_flags |= MAIN_SYSTEM_FLAG_DISPLAY_OFF;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
            level->result = 2;
            return;
        }
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_tutorial_mask_1_block_all_buttons(level);
        wldcore_menu_push_message_level(0x108C, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
    }
}
