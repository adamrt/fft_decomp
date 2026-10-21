/* Brave Story presents dispatched propositions, their participants and completed work histories. */

#include "fft/ability.h"
#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/display_object.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message_box.h"
#include "fft/proposition.h"
#include "fft/save.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Selected-row alias of MENU_STACK_CURSOR_SLOT_BRAVE_STORY_ADVANCING. A scalar declaration changes
 * the load order in brave_story_advancing_list_input_update. */
extern s32 g_brave_story_advancing_saved_cursor[];

/* The unit unit_panel_party_unit_prepare loads from a party slot for the status panels. */
extern unit_stats_t g_unit_panel_unit;

static void advancing_list_open(void);
static void participant_list_open(s32 proposition);
static void participant_detail_open(s32 proposition, s32 participant);
static void work_history_open(void);
static void work_history_entry_open(s32 value);

/* Push the Brave Story's job category menu (MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS).
 *
 * The active-jobs row is listed while any proposition is dispatched and the
 * completed-jobs row once any proposition is completed; each draws
 * TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST + row. The Brave Story menu
 * is hidden underneath. The cursor is restored from
 * g_brave_story_propositions_saved_cursor (reset when out of range).
 *
 * `value` holds the cursor and then the reloaded main-window index: as one
 * multi-set variable it is left to global allocation, which gives the
 * target's v1/v0 assignment in the tail. `unused` reproduces the 0x58 frame.
 */
void brave_story_propositions_level_open(void) {
    point32_t point;
    RECT bounds;
    s32 unused[4];
    point32_t* base;
    s32 index;
    s32 i;
    s32 completed;
    s32 depth;
    s32 value;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 9;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count = 0;
    completed = g_proposition_count;
    if (completed != 0) {
        g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entries[0]
            = BRAVE_STORY_PROPOSITION_ROW_ADVANCING;
        g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count++;
    }
    completed = 0;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_variable_get(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & PROPOSITION_VARIABLE_COMPLETED) {
            completed++;
        }
    }
    if (completed != 0) {
        g_menu_stack_records[g_menu_stack_depth]
            .brave_story_propositions
            .entries[g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count]
            = BRAVE_STORY_PROPOSITION_ROW_WORK_HISTORY;
        g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count++;
    }

    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.rows_image = index;
    bounds.x = 0x80;
    bounds.y = 0x80;
    bounds.w = 0x4C;
    bounds.h = g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 9;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_display_object_gs_image_pool[index].position;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count; i++) {
        window_display_text_entry(index,
            g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entries[i]
                + TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST,
            point, g_window_image_buffer);
        point.y += 0x10;
    }
    window_load_render_record_image(index, g_window_image_buffer);

    depth = g_menu_stack_depth;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].brave_story.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].brave_story.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[depth - 1].brave_story.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    if (g_menu_stack_records[depth].brave_story_propositions.entry_count
        < g_brave_story_propositions_saved_cursor[0] + 1) {
        g_brave_story_propositions_saved_cursor[0] = 0;
    }
    value = g_brave_story_propositions_saved_cursor[0];
    g_menu_stack_records[depth].brave_story_propositions.selected_entry = value;
    g_display_object_animated_pool[g_menu_stack_records[depth].brave_story_propositions.cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 6;
    g_menu_stack_depth = depth + 1;
    value = g_menu_stack_records[depth].brave_story_propositions.cursor_window;
    g_display_object_animated_pool[value].position.y
        = g_display_object_gs_image_pool[index].position.y - -(g_brave_story_propositions_saved_cursor[0] * 0x10) + 0xE;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS;
}

/* The proposition-category level's resume handler, undoing
 * brave_story_propositions_level_suspend: default palettes, the idle cursor, and the
 * list content record shown again (popping in) if the transition hid it. */
void brave_story_propositions_level_resume(menu_stack_record_t* level) {
    g_display_object_animated_pool[level->brave_story_propositions.cursor_window].palette
        = g_display_object_gs_image_pool[level->brave_story_propositions.rows_image].palette
        = g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 2].brave_story.casualty_image]
              .palette = 0;
    g_display_object_animated_pool[level->brave_story_propositions.cursor_window].sequence
        = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[level->brave_story_propositions.cursor_window].frame_index
        = g_display_object_animated_pool[level->brave_story_propositions.cursor_window].anim_counter = 0;
    if (g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 2].brave_story.casualty_image].flags
        & DISPLAY_OBJECT_HIDDEN) {
        g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 2].brave_story.casualty_image].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_world_map_flags &= ~WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
        g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 2].brave_story.casualty_image].flags
            |= DISPLAY_OBJECT_POP_IN;
    }
}

/* Activates the proposition-category cursor and render record: the window takes
 * palette 10 and its render record palette 2, the window's three 32-bit scratch fields are
 * cleared, and the current menu level's list content render record is hidden
 * while the transition runs. */
void brave_story_propositions_level_suspend(menu_stack_record_t* level) {
    g_display_object_animated_pool[level->brave_story_propositions.cursor_window].palette = 10;
    g_display_object_gs_image_pool[level->brave_story_propositions.rows_image].palette = 2;
    g_display_object_animated_pool[level->brave_story_propositions.cursor_window].sequence
        = DISPLAY_OBJECT_ANIM_CURSOR_STILL;
    g_display_object_animated_pool[level->brave_story_propositions.cursor_window].frame_index
        = g_display_object_animated_pool[level->brave_story_propositions.cursor_window].anim_counter = 0;
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 2].brave_story.casualty_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
}

/* Input step of the Brave Story's job category menu.
 *
 * Refreshes the displayed play time each frame, then, once the menu has
 * popped in: Select opens the highlighted row's help (message 0x105a + row),
 * Cross pops the level, Circle opens the active jobs list
 * (advancing_list_open) or the completed jobs list
 * (work_history_open), and Up and Down wrap the
 * cursor, moving the cursor window by 16 pixels a row. The wrap-around move
 * is spelled as a subtraction of a negative step to keep the window's y as
 * the first addition operand. */
void brave_story_propositions_input_update(menu_stack_record_t* level) {
    s32 window;
    s32 entry;
    s32 count;
    u32 buttons;

    g_hud_value = g_save_play_time_hours;
    g_hud_value_secondary = g_save_play_time_minutes;
    g_hud_value_tertiary = g_save_play_time_seconds;

    if ((g_display_object_gs_image_pool[level->brave_story_propositions.rows_image].flags & DISPLAY_OBJECT_POP_IN)
        != 0) {
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_display_object_animated_pool[level->brave_story_propositions.cursor_window].palette = 0xA;
        g_display_object_gs_image_pool[level->brave_story_propositions.rows_image].palette = 2;
        g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 2].brave_story.casualty_image].palette
            = 2;
        g_display_object_animated_pool[level->brave_story_propositions.cursor_window].sequence
            = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
        window = level->brave_story_propositions.cursor_window;
        g_display_object_animated_pool[window].anim_counter = 0;
        g_display_object_animated_pool[window].frame_index = 0;
        menu_stack_message_open(
            level->brave_story_propositions.entries[level->brave_story_propositions.selected_entry] + 0x105A, 1);
        return;
    }

    if ((buttons & PSX_PAD_CROSS) != 0 && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_brave_story_propositions_saved_cursor[0] = 0;
        g_display_object_animated_count -= 1;
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 2;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        return;
    }

    if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) != 0 && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_brave_story_propositions_saved_cursor[0] = level->brave_story_propositions.selected_entry;
        entry = level->brave_story_propositions.entries[level->brave_story_propositions.selected_entry];
        switch (entry) {
        case BRAVE_STORY_PROPOSITION_ROW_ADVANCING:
            brave_story_propositions_level_suspend(level);
            g_display_object_animated_pool[level->brave_story_propositions.cursor_window].sequence
                = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
            advancing_list_open();
            break;
        case BRAVE_STORY_PROPOSITION_ROW_WORK_HISTORY:
            brave_story_propositions_level_suspend(level);
            g_display_object_animated_pool[level->brave_story_propositions.cursor_window].sequence = entry;
            work_history_open();
            break;
        }
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (level->brave_story_propositions.entry_count >= 2) {
            if (level->brave_story_propositions.selected_entry != 0) {
                g_display_object_animated_pool[level->brave_story_propositions.cursor_window].position.y -= 0x10;
                level->brave_story_propositions.selected_entry = level->brave_story_propositions.selected_entry - 1;
            } else {
                g_display_object_animated_pool[level->brave_story_propositions.cursor_window].position.y
                    -= (level->brave_story_propositions.entry_count - 1) * -0x10;
                level->brave_story_propositions.selected_entry = level->brave_story_propositions.entry_count - 1;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }

    if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        count = level->brave_story_propositions.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->brave_story_propositions.selected_entry < count) {
                g_display_object_animated_pool[level->brave_story_propositions.cursor_window].position.y += 0x10;
                level->brave_story_propositions.selected_entry = level->brave_story_propositions.selected_entry + 1;
            } else {
                g_display_object_animated_pool[level->brave_story_propositions.cursor_window].position.y
                    -= count * 0x10;
                level->brave_story_propositions.selected_entry = 0;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }
}

/* Pushes the active jobs list (MENU_STACK_LEVEL_BRAVE_STORY_ADVANCING).
 *
 * Shows TEXT_ID_BRAVE_STORY_INTRO, shows the category menu's cursor
 * and panel again (popping the panel in) when they are hidden, then appends a
 * cursor window and three column-header windows plus one render record sized
 * to g_proposition_count rows. One row is seeded per dispatched
 * proposition, and the panel image is drawn in three passes: the proposition
 * name at x=8 (TEXT_ID_PROPOSITION_NAME_BASE | proposition_id), the
 * participant count at x=0x9c (TEXT_ID_VALUE) and the days remaining
 * at x=0xb6 (TEXT_ID_TWO_DIGIT_VALUE, assigned_days - elapsed_days);
 * both numeric columns go through g_text_substitution_values[0]. Finally the
 * saved cursor row
 * in g_brave_story_advancing_saved_cursor is clamped, the four windows are positioned relative to the
 * render record's origin, and the level is committed at depth + 1.
 *
 * `dead_locals` reserves 0x50 bytes to reproduce the target's 0xb8-byte frame;
 * the original local declarations are unknown.
 *
 * The seeding loop's entry test is spelled `i < count` (the target's signed
 * `blez`) around a do-while that re-reads the u8 count, which gives the
 * hoisted bound copy the target keeps in $a0.  The cursor row is folded as
 * base_y - row * -16 + 14 to keep base_y as the first addu operand.
 */
static void advancing_list_open(void) {
    point32_t point;
    RECT bounds;
    point32_t* base;
    s32 index;
    s32 depth;
    s32 i;
    s32 temp;
    s32 dead_locals[20];

    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);

    if (g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story_propositions.rows_image]
            .flags
        & DISPLAY_OBJECT_HIDDEN) {
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1]
                                           .brave_story_propositions.cursor_window]
            .flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story_propositions.rows_image]
            .flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story_propositions.rows_image]
            .flags |= DISPLAY_OBJECT_POP_IN;
    }

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    i = 0;
    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.title_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_PROPOSITION_TITLE_LABEL;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.count_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_PROPOSITION_COUNT_LABEL;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.days_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_PROPOSITION_DAYS_LABEL;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    /* One row per saved proposition, in save order. */
    g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count = 0;
    while (i < g_proposition_count) {
        g_menu_stack_records[g_menu_stack_depth]
            .proposition_list.entries[g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count] = i;
        i++;
        g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count++;
    }

    i = 0;
    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.render_index = index;
    bounds.y = 0x40;
    bounds.x = 0;
    bounds.w = 0xD8;
    bounds.h = g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 2, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 8;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_display_object_gs_image_pool[index].position;
    base->x = -0x60;
    base->y = -0x28;

    point.x = 8;
    point.y = 8;
    while (i < g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count) {
        window_display_text_entry(index,
            g_proposition_active[g_menu_stack_records[g_menu_stack_depth].proposition_list.entries[i]].proposition_id
                | TEXT_ID_PROPOSITION_NAME_BASE,
            point, g_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0x9C;
    point.y = 8;
    i = 0;
    while (i < g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count) {
        g_text_substitution_values[0]
            = g_proposition_active[g_menu_stack_records[g_menu_stack_depth].proposition_list.entries[i]]
                  .participant_count;
        window_display_text_entry(index, TEXT_ID_VALUE, point, g_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0xB6;
    point.y = 8;
    i = 0;
    while (i < g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count) {
        s32 entry = g_menu_stack_records[g_menu_stack_depth].proposition_list.entries[i];
        g_text_substitution_values[0]
            = g_proposition_active[entry].assigned_days - g_proposition_active[entry].elapsed_days;
        window_display_text_entry(index, TEXT_ID_TWO_DIGIT_VALUE, point, g_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    window_load_render_record_image(index, g_window_image_buffer);

    depth = g_menu_stack_depth;
    /* temp spans two blocks without crossing a call, so global allocation
     * gives the clamp flag and the cursor copy the target's v1.
     * A saved cursor past the last entry restarts at row 0. */
    temp = g_menu_stack_records[depth].proposition_list.entry_count < g_brave_story_advancing_saved_cursor[0] + 1;
    if (temp) {
        g_brave_story_advancing_saved_cursor[0] = 0;
    }
    temp = g_brave_story_advancing_saved_cursor[0];
    g_menu_stack_records[depth].proposition_list.cursor = temp;

    /* Place the windows on the list record: the cursor 16 pixels per saved row, the labels above their columns. */
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 6;
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.cursor_window].position.y
        = g_display_object_gs_image_pool[index].position.y - g_brave_story_advancing_saved_cursor[0] * -0x10 + 0xE;
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.title_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 3;
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.title_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.count_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 0x94;
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.count_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.days_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 0xB2;
    g_menu_stack_depth = depth + 1;
    g_display_object_animated_pool[g_menu_stack_records[depth].proposition_list.days_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY_ADVANCING;
}

/* Resume handler of the active-proposition list: restore the palettes of its four windows and
 * render record and idle the cursor, undoing brave_story_advancing_list_suspend. */
void brave_story_advancing_list_resume(menu_stack_record_t* level) {
    g_display_object_animated_pool[level->proposition_list.cursor_window].palette
        = g_display_object_animated_pool[level->proposition_list.title_window].palette
        = g_display_object_animated_pool[level->proposition_list.count_window].palette
        = g_display_object_animated_pool[level->proposition_list.days_window].palette
        = g_display_object_gs_image_pool[level->proposition_list.render_index].palette = 0;
    g_display_object_animated_pool[level->proposition_list.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[level->proposition_list.cursor_window].frame_index
        = g_display_object_animated_pool[level->proposition_list.cursor_window].anim_counter = 0;
}

/* Select the palettes for the active-proposition list and its four windows. */
void brave_story_advancing_list_suspend(menu_stack_record_t* level) {
    g_display_object_animated_pool[level->proposition_list.cursor_window].palette = 10;
    g_display_object_animated_pool[level->proposition_list.title_window].palette
        = g_display_object_animated_pool[level->proposition_list.count_window].palette
        = g_display_object_animated_pool[level->proposition_list.days_window].palette = 6;
    g_display_object_gs_image_pool[level->proposition_list.render_index].palette = 2;
}

/* Input step of the active jobs list. Select opens help message 0x105a, Cross
 * pops back to the category menu, and Circle releases the list and pushes the
 * chosen job's participant list. The wrap-around move is spelled as
 * a subtraction of a negative step: that keeps the window's y operand first
 * in the addition, as the target's register assignment requires. */
void brave_story_advancing_list_input_update(menu_stack_record_t* level) {
    u32 buttons;
    s32 count;
    s32 window;

    if ((g_display_object_gs_image_pool[level->proposition_list.render_index].flags & DISPLAY_OBJECT_POP_IN) != 0) {
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_display_object_animated_pool[level->proposition_list.cursor_window].sequence
            = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
        window = level->proposition_list.cursor_window;
        g_display_object_animated_pool[window].anim_counter = 0;
        g_display_object_animated_pool[window].frame_index = 0;
        brave_story_advancing_list_suspend(level);
        menu_stack_message_open(0x105A, 1);
        return;
    }

    if ((buttons & PSX_PAD_CROSS) != 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_brave_story_advancing_saved_cursor[0] = 0;
        g_display_object_animated_count -= 4;
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 5;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        return;
    }

    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_brave_story_advancing_saved_cursor[0] = level->proposition_list.cursor;
        g_display_object_animated_count -= 4;
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 5;
        g_menu_stack_depth -= 1;
        participant_list_open(level->proposition_list.entries[level->proposition_list.cursor]);
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (level->proposition_list.entry_count >= 2) {
            if (level->proposition_list.cursor != 0) {
                g_display_object_animated_pool[level->proposition_list.cursor_window].position.y -= 0x10;
                level->proposition_list.cursor = level->proposition_list.cursor - 1;
            } else {
                g_display_object_animated_pool[level->proposition_list.cursor_window].position.y
                    -= (level->proposition_list.entry_count - 1) * -0x10;
                level->proposition_list.cursor = level->proposition_list.entry_count - 1;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }

    if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        count = level->proposition_list.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->proposition_list.cursor < count) {
                g_display_object_animated_pool[level->proposition_list.cursor_window].position.y += 0x10;
                level->proposition_list.cursor = level->proposition_list.cursor + 1;
            } else {
                g_display_object_animated_pool[level->proposition_list.cursor_window].position.y -= count * 0x10;
                level->proposition_list.cursor = 0;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }
}

/* Pushes the participant list (MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANTS) of
 * g_proposition_active[proposition]: hides the job list underneath,
 * draws the job's detail panel with its units
 * (proposition_detail_panel_build) and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the job's name. The cursor
 * starts at the saved participant row.
 *
 * The 0x40 frame needs 32 bytes of locals the body never uses. The `- -`
 * spellings keep the target's operand order (render-record base first) that a
 * plain `+` of a loaded field loses to expand_binop's commutative swap. */
static void participant_list_open(s32 proposition) {
    /* Target frame evidence establishes 32 bytes of otherwise unused locals. */
    u8 stack_scratch[32];
    s32 depth;
    s32 window;

    proposition_unpack_row(&g_proposition_selected_row, g_proposition_active[proposition].proposition_id);
    g_menu_stack_records[g_menu_stack_depth].participant_list.proposition = proposition;
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story_propositions.cursor_window]
        .flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story_propositions.rows_image]
        .flags |= DISPLAY_OBJECT_HIDDEN;
    window = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].participant_list.cursor_window = window;
    g_display_object_animated_pool[window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[window].priority = 8;
    g_display_object_animated_pool[window].anim_counter = 0;
    g_display_object_animated_pool[window].frame_index = 0;
    proposition_detail_panel_build(&g_menu_stack_records[g_menu_stack_depth].proposition_detail_panel, proposition, 1);
    g_text_substitution_values[0] = g_proposition_active[proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    depth = g_menu_stack_depth;
    g_display_object_animated_pool[g_menu_stack_records[depth].participant_list.cursor_window].position.x
        = g_display_object_gs_image_pool[g_menu_stack_records[depth].participant_list.render_index].position.x + 0x38;
    g_display_object_animated_pool[g_menu_stack_records[depth].participant_list.cursor_window].position.y
        = g_display_object_gs_image_pool[g_menu_stack_records[depth].participant_list.render_index].position.y
        - -(g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_PARTICIPANTS].selected_index * 16)
        - -(g_menu_stack_records[depth].participant_list.panel_rows * 16 + 0xE);
    g_menu_stack_records[depth].participant_list.entry_count = g_proposition_active[proposition].participant_count;
    if (g_menu_stack_records[depth].participant_list.entry_count
        < g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_PARTICIPANTS].selected_index + 1) {
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_PARTICIPANTS].selected_index = 0;
    }
    g_menu_stack_records[depth].participant_list.cursor
        = g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_PARTICIPANTS].selected_index;
    g_menu_stack_depth = depth + 1;
    g_menu_stack_records[depth].participant_list.mode = MENU_STACK_WINDOW_MODE_BROWSE;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANTS;
}

/* Input step of the participant list.
 *
 * Nothing happens while the panel pops in. Cross tears the level down and
 * returns to the active jobs list; Circle saves the participant-list cursor and
 * closes the message box. Once it has gone, the level hands the row to
 * participant_detail_open. Both release the cursor window
 * and the panel. Up/down wrap the cursor and slide the cursor window by one
 * 16-pixel row, and do nothing when there are fewer than two rows.
 *
 * The two directional blocks mirror brave_story_advancing_list_input_update: the up wrap is spelled as
 * a subtraction of a negative step so the window's y stays the first addend,
 * and the down block decrements its own count in place. */
void brave_story_participant_list_input_update(menu_stack_record_t* level) {
    s32 count;

    if (g_display_object_gs_image_pool[level->participant_list.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        g_menu_stack_depth--;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        participant_detail_open(level->participant_list.proposition, level->participant_list.cursor);
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_PARTICIPANTS].selected_index = 0;
        g_display_object_animated_count--;
        g_display_object_image_count--;
        g_display_object_queue_count -= 2;
        g_menu_stack_depth--;
        advancing_list_open();
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_display_object_animated_count--;
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_PARTICIPANTS].selected_index = level->participant_list.cursor;
        g_display_object_image_count--;
        g_display_object_queue_count -= 2;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (level->participant_list.entry_count >= 2) {
            if (level->participant_list.cursor != 0) {
                g_display_object_animated_pool[level->participant_list.cursor_window].position.y -= 0x10;
                level->participant_list.cursor = level->participant_list.cursor - 1;
            } else {
                g_display_object_animated_pool[level->participant_list.cursor_window].position.y
                    -= (level->participant_list.entry_count - 1) * -0x10;
                level->participant_list.cursor = level->participant_list.entry_count - 1;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }

    if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        count = level->participant_list.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->participant_list.cursor < count) {
                g_display_object_animated_pool[level->participant_list.cursor_window].position.y += 0x10;
                level->participant_list.cursor = level->participant_list.cursor + 1;
            } else {
                g_display_object_animated_pool[level->participant_list.cursor_window].position.y -= count * 0x10;
                level->participant_list.cursor = 0;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }
}

/* Push the participant detail level (MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT):
 * the unit status panels of one unit sent on the job.
 *
 * Saves the 64x256 VRAM strip at (0x180, 0) to (0x200, 0x100) (the panels
 * draw over it; leaving the level moves it back), appends the L1 and R1
 * prompt windows, fills the panel data for the selected participant and
 * starts the three unit panel threads. Each window has its own pos/window
 * pointer: a single-set pointer keeps its address computation beside its
 * first use.
 */
static void participant_detail_open(s32 proposition, s32 participant) {
    RECT rect;
    display_object_animated_t* window1;
    display_object_animated_t* window2;
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
    g_menu_stack_records[g_menu_stack_depth].participant.proposition = proposition;
    g_menu_stack_records[g_menu_stack_depth].participant.participant = participant;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].participant.left_window = index;
    g_display_object_animated_pool[index].priority = 8;
    pos1 = &g_display_object_animated_pool[index].position;
    pos1->x = -0x74;
    pos1->y = -0x69;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_L1_BUTTON;
    window1 = &g_display_object_animated_pool[index];
    window1->anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].participant.right_window = index;
    g_display_object_animated_pool[index].priority = 8;
    pos2 = &g_display_object_animated_pool[index].position;
    pos2->x = 0x60;
    pos2->y = -0x69;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_R1_BUTTON;
    window2 = &g_display_object_animated_pool[index];
    window2->anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    unit_panel_party_unit_prepare(g_proposition_active[proposition].participant_indices[participant]);

    g_unit_panel_banner_params.position.x = 0;
    g_unit_panel_banner_params.position.y = 0;
    g_unit_panel_panel_params.position.y = 0;
    g_unit_panel_numbers_params.position.y = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_unit_panel_banner_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_panel_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_unit_panel_numbers_params, 0, 0);
    g_world_map_flags |= WORLD_MAP_FLAG_UNIT_PANEL_THREADS;

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].participant._unknown_08 = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT;
    g_menu_stack_depth = depth + 1;
}

/* Loads party member party_index into g_unit_panel_unit and
 * fills the status-panel records the participant detail level draws.
 *
 * Empty hand slots fall back to the shield; empty equipment reads as -1 and
 * a zero ability id is decremented to -1. Monsters show their primary
 * skillset's first five abilities instead of their equipped ability set. */
void unit_panel_party_unit_prepare(s32 party_index) {
    unit_stats_t* unit;
    unit_panel_summary_t* gauges;
    menu_unit_status_identity_t* identity;
    menu_status_panel_slot_state_t* detail;
    u16* abilities;
    s32 i;
    u8 flags;
    u16 maximum_hp;
    u16 maximum_mp;

    if (unit_party_slot_init(&g_unit_panel_unit, party_index, 1) == -1) {
        return;
    }

    unit = &g_unit_panel_unit;
    gauges = &g_unit_panel_selected_unit_stat_summary;
    identity = &g_formation_selected_unit_identity;
    detail = &g_formation_selected_unit_stat_detail.state;

    /* Team, level, experience and the HP, MP and CT gauges (a zero maximum
     * becomes 1), with the job, Brave, Faith and zodiac. */
    gauges->team_kind = 0;
    gauges->level = unit->level;
    flags = unit->team_flags;
    if (flags & UNIT_TEAM_MASK) {
        gauges->team_kind = 1;
    }

    if (!(flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
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

    /* Movement, attack and evasion; each shield evade is the better hand's. */
    detail->move = unit->move;
    detail->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    detail->jump = unit->jump;
    detail->right_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    detail->left_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    detail->right_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    detail->left_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    detail->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    detail->physical_class_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    detail->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE]) {
        detail->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    }

    detail->physical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    detail->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    detail->magical_class_evade = 0;
    detail->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE]) {
        detail->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    }

    detail->magical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

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
        abilities = ability_skillset_list_build(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
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
void brave_story_participant_level_resume(menu_stack_record_t* level) {
    s32 second = level->participant.left_window;

    g_display_object_animated_pool[level->participant.right_window].palette = 0;
    g_display_object_animated_pool[second].palette = 0;
}

/* The g_level_suspend_handlers entry of MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT: nothing to suspend. */
void brave_story_participant_level_suspend(menu_stack_record_t* unused_level) {
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
void brave_story_participant_input_update(menu_stack_record_t* level) {
    RECT rect;
    u32 buttons;
    s32 count;
    s32 direction;
    s32 participant;

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) == 0) {
        rect.x = 0x200;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0x100;
        g_world_map_flags &= ~(WORLD_MAP_FLAG_INPUT_BLOCKED | WORLD_MAP_FLAG_UNIT_PANEL_THREADS);
        g_menu_stack_depth--;
        MoveImage(&rect, 0x180, 0);
        DrawSync(0);
        g_text_substitution_values[0]
            = g_proposition_active[level->participant.proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
        participant_list_open(level->participant.proposition);
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_display_object_animated_count -= 2;
        g_display_object_queue_count -= 2;
        thread_parameters_set(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
        thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
        thread_parameters_set(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        g_display_object_animated_pool[level->participant.left_window].palette
            = g_display_object_animated_pool[level->participant.right_window].palette = 0xA;
        sound_play_effect(SOUND_SFX_CONFIRM);
        unit_panel_help_open();
        return;
    }

    if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
        g_display_object_animated_pool[level->participant.left_window].sequence = DISPLAY_OBJECT_ANIM_L1_BUTTON;
        g_display_object_animated_pool[level->participant.right_window].sequence = DISPLAY_OBJECT_ANIM_R1_BUTTON;
        g_display_object_animated_pool[level->participant.left_window].frame_index
            = g_display_object_animated_pool[level->participant.left_window].anim_counter
            = g_display_object_animated_pool[level->participant.right_window].frame_index
            = g_display_object_animated_pool[level->participant.right_window].anim_counter = 0;
    }

    if (world_map_input_repeat_update(PSX_PAD_R1) != 0
        && g_display_object_animated_pool[level->participant.left_window].sequence
            != DISPLAY_OBJECT_ANIM_L1_BUTTON_PRESS) {
        g_display_object_animated_pool[level->participant.right_window].sequence = DISPLAY_OBJECT_ANIM_R1_BUTTON_PRESS;
        g_display_object_animated_pool[level->participant.right_window].frame_index
            = g_display_object_animated_pool[level->participant.right_window].anim_counter = 0;
        count = g_proposition_active[level->participant.proposition].participant_count;
        if (count == 1) {
            /* Shared tail: both single-participant cases branch to the closing
             * sound effect; nesting the redraw under `count != 1` changes the
             * register allocation. */
            goto done;
        }
        participant = level->participant.participant + 1;
        if (participant >= count) {
            level->participant.participant = 0;
        } else {
            level->participant.participant = participant;
        }
        direction = 1;
    } else {
        if (world_map_input_repeat_update(PSX_PAD_L1) == 0
            || g_display_object_animated_pool[level->participant.right_window].sequence
                == DISPLAY_OBJECT_ANIM_R1_BUTTON_PRESS) {
            return;
        }
        g_display_object_animated_pool[level->participant.left_window].sequence = DISPLAY_OBJECT_ANIM_L1_BUTTON_PRESS;
        g_display_object_animated_pool[level->participant.left_window].frame_index
            = g_display_object_animated_pool[level->participant.left_window].anim_counter = 0;
        if (g_proposition_active[level->participant.proposition].participant_count == 1) {
            goto done;
        }
        if (level->participant.participant != 0) {
            level->participant.participant--;
        } else {
            level->participant.participant = g_proposition_active[level->participant.proposition].participant_count - 1;
        }
        direction = 2;
    }

    g_unit_panel_banner_params.redraw_request = direction;
    unit_panel_party_unit_prepare(
        g_proposition_active[level->participant.proposition].participant_indices[level->participant.participant]);
    g_unit_panel_panel_params.redraw_request = 1;
    g_unit_panel_numbers_params.redraw_request = 1;
done:
    sound_play_effect(SOUND_SFX_PAGE_SWITCH);
}

/* Push the Brave Story list of completed propositions: show its intro message, list each
 * completed proposition's name in a panel of up to 8 visible rows at its saved cursor,
 * and hide the parent list's content while it is open. */
static void work_history_open(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* colors;
    s32 count;
    s32 i;
    s32 record_index;

    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    colors = g_menu_stack_row_text_colors;
    values = g_menu_stack_entry_values;
    for (; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_variable_get(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & PROPOSITION_VARIABLE_COMPLETED) {
            *values = i + TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE;
            *colors = 0;
            colors++;
            values++;
            count++;
        }
    }
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
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_WORK_HISTORY, count, i);
    window_panel_thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 8;
    record_index = g_menu_stack_depth;
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    g_display_object_gs_image_pool[g_menu_stack_records[record_index - 1].brave_story_propositions.retained_image_index]
        .flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY;
    g_menu_stack_depth = record_index + 1;
}

/* The g_level_resume_handlers entry of MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY: nothing to restore. */
void brave_story_work_history_resume(menu_stack_record_t* unused_level) {
}

/* The g_level_suspend_handlers entry of MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY: nothing to suspend. */
void brave_story_work_history_suspend(menu_stack_record_t* unused_level) {
}

/* Input step of the completed jobs list opened by
 * work_history_open: twin of
 * brave_story_unexplored_land_list_input_update with help message 0x105b and no
 * file-loading gate. Closing without a selection pops in the image index retained at +0x0c
 * in the category slot before resuming the category menu; a
 * confirmed entry goes to work_history_entry_open. */
void brave_story_work_history_input_update(menu_stack_record_t* level) {
    s32 depth;

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED)
        && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        depth = g_menu_stack_depth;
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth = depth - 1;
        if (level->panel.result != 0) {
            menu_stack_cursor_store(MENU_STACK_CURSOR_SLOT_WORK_HISTORY);
            work_history_entry_open(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story_propositions.retained_image_index]
            .flags |= DISPLAY_OBJECT_POP_IN;
        menu_stack_cursor_clear(MENU_STACK_CURSOR_SLOT_WORK_HISTORY);
        menu_stack_dispatch_resume();
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        menu_stack_message_open(0x105B, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY, the text of a
 * completed job; brave_story_wait_work_history_then_pop closes it.
 * Records the entry on the new level, reads the date the job was last done
 * from g_proposition_last_attempt_date_bits, and sets up
 * g_text_window on the job's text
 * (TEXT_ID_WORLD_MESSAGE_BASE + entry) with the month name
 * (TEXT_ID_MONTH_NAME_BASE + month) and the day substituted. Then
 * hides the category menu's window and frame and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the job's name.
 *
 * Matching constraint: the constant switch, with the selected case last,
 * keeps image_position.y at its absolute address while later stores reuse
 * the text_id base. This spelling does not establish the original source.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
static void work_history_entry_open(s32 value) {
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    file_pending_wait();
    g_menu_stack_records[g_menu_stack_depth].work_history_entry = value;
    calendar_date_read(g_proposition_last_attempt_date_bits, value, &date);
    g_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 0x60;
        break;
    case 1:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 0x70;
        break;
    }
    g_text_window.base.x = -122;
    g_text_window.base.y = -40;
    g_text_window.text_width = 0xdc;
    g_text_window.rows_per_page = 8;
    g_text_window.priority = 8;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[0] = date.month + TEXT_ID_MONTH_NAME_BASE;
    g_text_window.text_substitutions[1] = date.day;
    text_window_open(&g_text_window);
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story_propositions.cursor_window]
        .flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story_propositions.rows_image]
        .flags |= DISPLAY_OBJECT_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Close a completed job's text level once its window has popped in and
 * text_window_update reports it closed: show the category
 * menu's window and frame again (replaying the frame's pop-in), pop the
 * stack and reopen the completed jobs list. Mirrors
 * text_level_update. */
void brave_story_wait_work_history_then_pop(menu_stack_record_t* unused_level) {
    s32* index = &g_text_window.render_record_index;
    s32 depth;

    if (g_display_object_gs_image_pool[*index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (text_window_update(&g_text_window) != 0) {
        return;
    }
    depth = g_menu_stack_depth;
    g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story_propositions.cursor_window].flags
        &= ~DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story_propositions.rows_image].flags
        &= ~DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth - 1;
    g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story_propositions.rows_image].flags
        |= DISPLAY_OBJECT_POP_IN;
    work_history_open();
}
