/* List levels, including tint transitions, proposition categories and active propositions, participant lists and
 * details, Brave Story propositions and attempt details, screen transitions, the bar menu's suspend handler, and the
 * tutorial category, formation and battle tutorial lists. */

#include "fft/ability.h"
#include "fft/bits.h"
#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message.h"
#include "fft/message_box.h"
#include "fft/music.h"
#include "fft/proposition.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static void tutorial_build_menu_panel(menu_stack_window_level_t* panel);
static void brave_story_open_advancing_list(void);
static void brave_story_open_work_history(void);
static void tutorial_open_formation_list(void);
static void tutorial_push_menu_level(s32 selected_entry, s32 reset);
static void tutorial_open_battle_list(void);
static void brave_story_push_participant_level(s32 proposition, s32 participant);
static void brave_story_push_participant_list_level(s32 proposition);
static void brave_story_push_work_history_level(s32 value);
static void menu_stack_close_parent_list_windows(void);
static void menu_stack_close_parent_list_frame(void);

/* The proposition-category level's resume handler, undoing
 * brave_story_suspend_propositions_level: default palettes, the idle cursor, and the
 * list content record shown again (popping in) if the transition hid it. */
void brave_story_resume_propositions_level(menu_stack_record_t* level) {
    g_window_records[level->list_window.main_window].palette
        = g_window_gs_image_records[level->list_window.side_window].palette
        = g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 2].list_window.content_render].palette
        = 0;
    g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[level->list_window.main_window].frame_index
        = g_window_records[level->list_window.main_window].anim_counter = 0;
    if (g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 2].list_window.content_render].flags
        & DISPLAY_OBJECT_HIDDEN) {
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 2].list_window.content_render].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_system_flags &= ~SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 2].list_window.content_render].flags
            |= DISPLAY_OBJECT_POP_IN;
    }
}

/* Activates the proposition-category cursor and render record: the window takes
 * palette 10 and its render record palette 2, the window's three 32-bit scratch fields are
 * cleared, and the current menu level's list content render record is hidden
 * while the transition runs. */
void brave_story_suspend_propositions_level(menu_stack_record_t* level) {
    g_window_records[level->list_window.main_window].palette = 10;
    g_window_gs_image_records[level->list_window.side_window].palette = 2;
    g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_STILL;
    g_window_records[level->list_window.main_window].frame_index
        = g_window_records[level->list_window.main_window].anim_counter = 0;
    g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 2].list_window.content_render].flags
        |= DISPLAY_OBJECT_HIDDEN;
}

/* Input step of the Brave Story's job category menu.
 *
 * Refreshes the displayed play time each frame, then, once the menu has
 * popped in: Select opens the highlighted row's help (message 0x105a + row),
 * Cross pops the level, Circle opens the active jobs list
 * (brave_story_open_advancing_list) or the completed jobs list
 * (brave_story_open_work_history), and Up and Down wrap the
 * cursor, moving the cursor window by 16 pixels a row. The wrap-around move
 * is spelled as a subtraction of a negative step to keep the window's y as
 * the first addition operand. */
void brave_story_handle_propositions_input(menu_stack_record_t* level) {
    s32 window;
    s32 entry;
    s32 count;
    u32 buttons;

    g_hud_value = g_save_play_time_hours;
    g_hud_value_secondary = g_save_play_time_minutes;
    g_hud_value_tertiary = g_save_play_time_seconds;

    if ((g_window_gs_image_records[level->list_window.side_window].flags & DISPLAY_OBJECT_POP_IN) != 0) {
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_window_records[level->list_window.main_window].palette = 0xA;
        g_window_gs_image_records[level->list_window.side_window].palette = 2;
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 2].list_window.content_render].palette = 2;
        g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
        window = level->list_window.main_window;
        g_window_records[window].anim_counter = 0;
        g_window_records[window].frame_index = 0;
        menu_stack_push_message_level(level->list_window.entries[level->list_window.selected_entry] + 0x105A, 1);
        return;
    }

    if ((buttons & PSX_PAD_CROSS) != 0 && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_brave_story_propositions_saved_cursor[0] = 0;
        g_window_record_count -= 1;
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 2;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        return;
    }

    if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) != 0 && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_brave_story_propositions_saved_cursor[0] = level->list_window.selected_entry;
        entry = level->list_window.entries[level->list_window.selected_entry];
        switch (entry) {
        case BRAVE_STORY_PROPOSITION_ROW_ADVANCING:
            brave_story_suspend_propositions_level(level);
            g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            brave_story_open_advancing_list();
            break;
        case BRAVE_STORY_PROPOSITION_ROW_WORK_HISTORY:
            brave_story_suspend_propositions_level(level);
            g_window_records[level->list_window.main_window].sequence = entry;
            brave_story_open_work_history();
            break;
        }
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (level->list_window.entry_count >= 2) {
            if (level->list_window.selected_entry != 0) {
                g_window_records[level->list_window.main_window].position.y -= 0x10;
                level->list_window.selected_entry = level->list_window.selected_entry - 1;
            } else {
                g_window_records[level->list_window.main_window].position.y
                    -= (level->list_window.entry_count - 1) * -0x10;
                level->list_window.selected_entry = level->list_window.entry_count - 1;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }

    if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        count = level->list_window.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->list_window.selected_entry < count) {
                g_window_records[level->list_window.main_window].position.y += 0x10;
                level->list_window.selected_entry = level->list_window.selected_entry + 1;
            } else {
                g_window_records[level->list_window.main_window].position.y -= count * 0x10;
                level->list_window.selected_entry = 0;
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
 * The 0xb8 frame carries 0x50 bytes of locals the original never reads;
 * `dead_locals` reproduces them.
 *
 * The seeding loop's entry test is spelled `i < count` (the target's signed
 * `blez`) around a do-while that re-reads the u8 count, which gives the
 * hoisted bound copy the target keeps in $a0.  The cursor row is folded as
 * base_y - row * -16 + 14 to keep base_y as the first addu operand.
 */
static void brave_story_open_advancing_list(void) {
    point32_t point;
    RECT bounds;
    point32_t* base;
    s32 index;
    s32 depth;
    s32 i;
    s32 temp;
    s32 dead_locals[20];

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);

    if (g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].proposition_list.title_window].flags
        & DISPLAY_OBJECT_HIDDEN) {
        g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].proposition_list.cursor_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].proposition_list.title_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].proposition_list.title_window].flags
            |= DISPLAY_OBJECT_POP_IN;
    }

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    i = 0;
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.title_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_PROPOSITION_TITLE_LABEL;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.count_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_PROPOSITION_COUNT_LABEL;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.days_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_PROPOSITION_DAYS_LABEL;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    /* One row per saved proposition, in save order. */
    g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count = 0;
    while (i < g_proposition_count) {
        g_menu_stack_records[g_menu_stack_depth]
            .proposition_list.entries[g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count] = i;
        i++;
        g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count++;
    }

    i = 0;
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_list.render_index = index;
    bounds.y = 0x40;
    bounds.x = 0;
    bounds.w = 0xD8;
    bounds.h = g_menu_stack_records[g_menu_stack_depth].proposition_list.entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 2, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 8;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_window_gs_image_records[index].base;
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
    g_window_records[g_menu_stack_records[depth].proposition_list.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 6;
    g_window_records[g_menu_stack_records[depth].proposition_list.cursor_window].position.y
        = g_window_gs_image_records[index].base.y - g_brave_story_advancing_saved_cursor[0] * -0x10 + 0xE;
    g_window_records[g_menu_stack_records[depth].proposition_list.title_window].position.x
        = g_window_gs_image_records[index].base.x + 3;
    g_window_records[g_menu_stack_records[depth].proposition_list.title_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
    g_window_records[g_menu_stack_records[depth].proposition_list.count_window].position.x
        = g_window_gs_image_records[index].base.x + 0x94;
    g_window_records[g_menu_stack_records[depth].proposition_list.count_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
    g_window_records[g_menu_stack_records[depth].proposition_list.days_window].position.x
        = g_window_gs_image_records[index].base.x + 0xB2;
    g_menu_stack_depth = depth + 1;
    g_window_records[g_menu_stack_records[depth].proposition_list.days_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY_ADVANCING;
}

/* Resume handler of the active-proposition list: restore the palettes of its four windows and
 * render record and idle the cursor, undoing brave_story_suspend_advancing_list. */
void brave_story_resume_advancing_list(menu_stack_record_t* level) {
    g_window_records[level->proposition_list.cursor_window].palette
        = g_window_records[level->proposition_list.title_window].palette
        = g_window_records[level->proposition_list.count_window].palette
        = g_window_records[level->proposition_list.days_window].palette
        = g_window_gs_image_records[level->proposition_list.render_index].palette = 0;
    g_window_records[level->proposition_list.cursor_window].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[level->proposition_list.cursor_window].frame_index
        = g_window_records[level->proposition_list.cursor_window].anim_counter = 0;
}

/* Select the palettes for the active-proposition list and its four windows. */
void brave_story_suspend_advancing_list(menu_stack_record_t* level) {
    g_window_records[level->proposition_list.cursor_window].palette = 10;
    g_window_records[level->proposition_list.title_window].palette
        = g_window_records[level->proposition_list.count_window].palette
        = g_window_records[level->proposition_list.days_window].palette = 6;
    g_window_gs_image_records[level->proposition_list.render_index].palette = 2;
}

/* Input step of the active jobs list. Select opens help message 0x105a, Cross
 * pops back to the category menu, and Circle releases the list and pushes the
 * chosen job's participant list. The wrap-around move is spelled as
 * a subtraction of a negative step: that keeps the window's y operand first
 * in the addition, as the target's register assignment requires. */
void brave_story_handle_advancing_list_input(menu_stack_record_t* level) {
    u32 buttons;
    s32 count;
    s32 window;

    if ((g_window_gs_image_records[level->proposition_list.render_index].flags & DISPLAY_OBJECT_POP_IN) != 0) {
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_window_records[level->proposition_list.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
        window = level->proposition_list.cursor_window;
        g_window_records[window].anim_counter = 0;
        g_window_records[window].frame_index = 0;
        brave_story_suspend_advancing_list(level);
        menu_stack_push_message_level(0x105A, 1);
        return;
    }

    if ((buttons & PSX_PAD_CROSS) != 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_brave_story_advancing_saved_cursor[0] = 0;
        g_window_record_count -= 4;
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 5;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        return;
    }

    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_brave_story_advancing_saved_cursor[0] = level->proposition_list.cursor;
        g_window_record_count -= 4;
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 5;
        g_menu_stack_depth -= 1;
        brave_story_push_participant_list_level(level->proposition_list.entries[level->proposition_list.cursor]);
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (level->proposition_list.entry_count >= 2) {
            if (level->proposition_list.cursor != 0) {
                g_window_records[level->proposition_list.cursor_window].position.y -= 0x10;
                level->proposition_list.cursor = level->proposition_list.cursor - 1;
            } else {
                g_window_records[level->proposition_list.cursor_window].position.y
                    -= (level->proposition_list.entry_count - 1) * -0x10;
                level->proposition_list.cursor = level->proposition_list.entry_count - 1;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }

    if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        count = level->proposition_list.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->proposition_list.cursor < count) {
                g_window_records[level->proposition_list.cursor_window].position.y += 0x10;
                level->proposition_list.cursor = level->proposition_list.cursor + 1;
            } else {
                g_window_records[level->proposition_list.cursor_window].position.y -= count * 0x10;
                level->proposition_list.cursor = 0;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }
}

/* Pushes the participant list (MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANTS) of
 * g_proposition_active[proposition]: hides the job list underneath,
 * draws the job's detail panel with its units
 * (proposition_build_detail_panel) and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the job's name. The cursor
 * starts at the saved participant row.
 *
 * The 0x40 frame needs 32 bytes of locals the body never uses. The `- -`
 * spellings keep the target's operand order (render-record base first) that a
 * plain `+` of a loaded field loses to expand_binop's commutative swap. */
static void brave_story_push_participant_list_level(s32 proposition) {
    /* Target frame evidence establishes 32 bytes of otherwise unused locals. */
    u8 stack_scratch[32];
    s32 depth;
    s32 window;

    proposition_unpack_row(&g_proposition_selected_row, g_proposition_active[proposition].proposition_id);
    g_menu_stack_records[g_menu_stack_depth].participant_list.proposition = proposition;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].window_render.window_index].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].window_render.render_index].flags
        |= DISPLAY_OBJECT_HIDDEN;
    window = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].participant_list.cursor_window = window;
    g_window_records[window].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[window].priority = 8;
    g_window_records[window].anim_counter = 0;
    g_window_records[window].frame_index = 0;
    proposition_build_detail_panel(&g_menu_stack_records[g_menu_stack_depth], proposition, 1);
    g_text_substitution_values[0] = g_proposition_active[proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    depth = g_menu_stack_depth;
    g_window_records[g_menu_stack_records[depth].participant_list.cursor_window].position.x
        = g_window_gs_image_records[g_menu_stack_records[depth].participant_list.render_index].base.x + 0x38;
    g_window_records[g_menu_stack_records[depth].participant_list.cursor_window].position.y
        = g_window_gs_image_records[g_menu_stack_records[depth].participant_list.render_index].base.y
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
 * returns to the active jobs list; Circle records the row in
 * g_brave_story_participant_saved_cursor and closes the message box, and once
 * it has gone the level hands the row to
 * brave_story_push_participant_level. Both release the cursor window
 * and the panel. Up/down wrap the cursor and slide the cursor window by one
 * 16-pixel row, and do nothing when there are fewer than two rows.
 *
 * The two directional blocks mirror brave_story_handle_advancing_list_input: the up wrap is spelled as
 * a subtraction of a negative step so the window's y stays the first addend,
 * and the down block decrements its own count in place. */
void brave_story_handle_participant_list_input(menu_stack_record_t* level) {
    s32 count;

    if (g_window_gs_image_records[level->participant_list.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        g_menu_stack_depth--;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        brave_story_push_participant_level(level->participant_list.proposition, level->participant_list.cursor);
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_brave_story_participant_saved_cursor = 0;
        g_window_record_count--;
        g_window_render_record_count--;
        g_window_render_object_count -= 2;
        g_menu_stack_depth--;
        brave_story_open_advancing_list();
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_window_record_count--;
        g_brave_story_participant_saved_cursor = level->participant_list.cursor;
        g_window_render_record_count--;
        g_window_render_object_count -= 2;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (level->participant_list.entry_count >= 2) {
            if (level->participant_list.cursor != 0) {
                g_window_records[level->participant_list.cursor_window].position.y -= 0x10;
                level->participant_list.cursor = level->participant_list.cursor - 1;
            } else {
                g_window_records[level->participant_list.cursor_window].position.y
                    -= (level->participant_list.entry_count - 1) * -0x10;
                level->participant_list.cursor = level->participant_list.entry_count - 1;
            }
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        }
    }

    if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        count = level->participant_list.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->participant_list.cursor < count) {
                g_window_records[level->participant_list.cursor_window].position.y += 0x10;
                level->participant_list.cursor = level->participant_list.cursor + 1;
            } else {
                g_window_records[level->participant_list.cursor_window].position.y -= count * 0x10;
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
static void brave_story_push_participant_level(s32 proposition, s32 participant) {
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

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].participant.left_window = index;
    g_window_records[index].priority = 8;
    pos1 = &g_window_records[index].position;
    pos1->x = -0x74;
    pos1->y = -0x69;
    g_window_records[index].sequence = WINDOW_ANIM_L1_BUTTON;
    window1 = &g_window_records[index];
    window1->anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].participant.right_window = index;
    g_window_records[index].priority = 8;
    pos2 = &g_window_records[index].position;
    pos2->x = 0x60;
    pos2->y = -0x69;
    g_window_records[index].sequence = WINDOW_ANIM_R1_BUTTON;
    window2 = &g_window_records[index];
    window2->anim_counter = 0;
    g_window_records[index].frame_index = 0;

    unit_panel_build_unit_data(g_proposition_active[proposition].participant_indices[participant]);

    g_unit_panel_banner_params.x = 0;
    g_unit_panel_banner_params.y = 0;
    g_unit_panel_panel_params.y = 0;
    g_unit_panel_numbers_params.y = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_unit_panel_banner_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_panel_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_unit_panel_numbers_params, 0, 0);
    g_system_flags |= SYSTEM_FLAG_UNIT_PANEL_THREADS;

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
void unit_panel_build_unit_data(s32 party_index) {
    unit_stats_t* unit;
    unit_panel_summary_t* gauges;
    menu_unit_status_identity_t* identity;
    menu_status_panel_slot_state_t* detail;
    u16* abilities;
    s32 i;
    u8 flags;
    u16 maximum_hp;
    u16 maximum_mp;

    if (unit_init_from_party_slot(&g_unit_panel_unit, party_index, 1) == -1) {
        return;
    }

    unit = &g_unit_panel_unit;
    gauges = &g_unit_panel_selected_unit_stat_summary;
    identity = &g_formation_selected_unit_identity;
    detail = &g_formation_selected_unit_stat_detail.state;

    /* Team, level, experience and the HP, MP and CT gauges (a zero maximum
     * becomes 1), with the job, bravery, faith and zodiac. */
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
        abilities = ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
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
void brave_story_resume_participant_level(menu_stack_record_t* level) {
    s32 second = level->participant.left_window;

    g_window_records[level->participant.right_window].palette = 0;
    g_window_records[second].palette = 0;
}

/* The g_menu_stack_suspend_handlers entry of MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT: nothing to suspend. */
void brave_story_suspend_participant_level(menu_stack_record_t* unused_level) {
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
void brave_story_handle_participant_input(menu_stack_record_t* level) {
    RECT rect;
    u32 buttons;
    s32 count;
    s32 direction;
    s32 participant;

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) == 0) {
        rect.x = 0x200;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0x100;
        g_system_flags &= ~(SYSTEM_FLAG_INPUT_BLOCKED | SYSTEM_FLAG_UNIT_PANEL_THREADS);
        g_menu_stack_depth--;
        MoveImage(&rect, 0x180, 0);
        DrawSync(0);
        g_text_substitution_values[0]
            = g_proposition_active[level->participant.proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
        brave_story_push_participant_list_level(level->participant.proposition);
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        sound_play_effect(SOUND_SFX_CANCEL);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        g_window_record_count -= 2;
        g_window_render_object_count -= 2;
        thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        g_window_records[level->participant.left_window].palette
            = g_window_records[level->participant.right_window].palette = 0xA;
        sound_play_effect(SOUND_SFX_CONFIRM);
        unit_panel_push_help_level();
        return;
    }

    if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
        g_window_records[level->participant.left_window].sequence = WINDOW_ANIM_L1_BUTTON;
        g_window_records[level->participant.right_window].sequence = WINDOW_ANIM_R1_BUTTON;
        g_window_records[level->participant.left_window].frame_index
            = g_window_records[level->participant.left_window].anim_counter
            = g_window_records[level->participant.right_window].frame_index
            = g_window_records[level->participant.right_window].anim_counter = 0;
    }

    if (world_map_update_input_repeat(PSX_PAD_R1) != 0
        && g_window_records[level->participant.left_window].sequence != WINDOW_ANIM_L1_BUTTON_PRESS) {
        g_window_records[level->participant.right_window].sequence = WINDOW_ANIM_R1_BUTTON_PRESS;
        g_window_records[level->participant.right_window].frame_index
            = g_window_records[level->participant.right_window].anim_counter = 0;
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
        if (world_map_update_input_repeat(PSX_PAD_L1) == 0
            || g_window_records[level->participant.right_window].sequence == WINDOW_ANIM_R1_BUTTON_PRESS) {
            return;
        }
        g_window_records[level->participant.left_window].sequence = WINDOW_ANIM_L1_BUTTON_PRESS;
        g_window_records[level->participant.left_window].frame_index
            = g_window_records[level->participant.left_window].anim_counter = 0;
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
    unit_panel_build_unit_data(
        g_proposition_active[level->participant.proposition].participant_indices[level->participant.participant]);
    g_unit_panel_panel_params.redraw_request = 1;
    g_unit_panel_numbers_params.redraw_request = 1;
done:
    sound_play_effect(SOUND_SFX_PAGE_SWITCH);
}

/* Push the Brave Story list of completed propositions: show its intro message, list each
 * completed proposition's name in a panel of up to 8 visible rows at its saved cursor,
 * and hide the parent list's content while it is open. */
static void brave_story_open_work_history(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    flags = g_menu_stack_row_flags;
    values = g_menu_stack_entry_values;
    for (; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & PROPOSITION_VARIABLE_COMPLETED) {
            *values = i + TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE;
            *flags = 0;
            flags++;
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
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 8;
    record_index = g_menu_stack_depth;
    g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_window_gs_image_records[g_menu_stack_records[record_index - 1].list_window.content_render].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY;
    g_menu_stack_depth = record_index + 1;
}

/* The g_menu_stack_resume_handlers entry of MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY: nothing to restore. */
void brave_story_resume_work_history(menu_stack_record_t* unused_level) {
}

/* The g_menu_stack_suspend_handlers entry of MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY: nothing to suspend. */
void brave_story_suspend_work_history(menu_stack_record_t* unused_level) {
}

/* Input step of the completed jobs list opened by
 * brave_story_open_work_history: twin of
 * brave_story_handle_unexplored_land_list_input with help message 0x105b and no
 * file-loading gate. Closing without a selection pops the Brave Story menu's
 * injured/casualties record back in before resuming the category menu; a
 * confirmed entry goes to brave_story_push_work_history_level. */
void brave_story_handle_work_history_input(menu_stack_record_t* level) {
    s32 depth;

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        depth = g_menu_stack_depth;
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth = depth - 1;
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_WORK_HISTORY);
            brave_story_push_work_history_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_window_gs_image_records[g_menu_stack_records[depth - 2].list_window.content_render].flags
            |= DISPLAY_OBJECT_POP_IN;
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_WORK_HISTORY);
        menu_stack_dispatch_resume();
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        menu_stack_push_message_level(0x105B, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
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
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
static void brave_story_push_work_history_level(s32 value) {
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    file_wait_for_pending();
    g_menu_stack_records[g_menu_stack_depth].window_index = value;
    calendar_read_date(g_proposition_last_attempt_date_bits, value, &date);
    g_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x60;
        break;
    case 1:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x70;
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
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].window_render.window_index].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].window_render.render_index].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Close a completed job's text level once its window has popped in and
 * text_window_step reports it closed: show the category
 * menu's window and frame again (replaying the frame's pop-in), pop the
 * stack and reopen the completed jobs list. Mirrors
 * menu_stack_wait_text_then_pop. */
void brave_story_wait_work_history_then_pop(menu_stack_record_t* unused_level) {
    s32* index = &g_text_window.render_record_index;
    s32 depth;

    if (g_window_gs_image_records[*index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (text_window_step(&g_text_window) != 0) {
        return;
    }
    depth = g_menu_stack_depth;
    g_window_records[g_menu_stack_records[depth - 2].window_render.window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[depth - 2].window_render.render_index].flags
        &= ~DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth - 1;
    g_window_gs_image_records[g_menu_stack_records[depth - 2].window_render.render_index].flags
        |= DISPLAY_OBJECT_POP_IN;
    brave_story_open_work_history();
}

/* Push a screen transition into `screen` (a menu level type): clear the map, hide date, location label and
 * value, fade in, do that screen's setup (music, list, message block), clear the scratch buffer
 * and start the zoom thread; param_a and param_b are kept for the new screen. */
void screen_transition_push_level(s32 screen, s32 param_a, s32 param_b) {
    u32* clear;
    s32 count;
    s32 i;
    s32 depth;

    world_map_clear_screen_and_set_clip_rect(1);
    g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE | SYSTEM_FLAG_HIDE_CALENDAR_DATE | SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    world_map_fade_start(0, 0x10);
    switch (screen) {
    case MENU_STACK_LEVEL_BRAVE_STORY:
        window_set_render_objects_visible(0);
        audio_queue_wait_until_empty();
        audio_queue_start_music(AUDIO_MUSIC(2, MUSIC_BRAVE_STORY));
        break;
    case MENU_STACK_LEVEL_BRAVE_STORY_EVENTS:
        window_set_render_objects_visible(0);
        screen_set_entry_4_cursor(2);
        count = 0;
        audio_queue_wait_until_empty();
        audio_queue_start_music(AUDIO_MUSIC(2, MUSIC_BRAVE_STORY));
        world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth - 1]);
        if (script_get_variable(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
            for (i = 0; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
                if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
                    count++;
                }
            }
        }
        if (count != 0) {
            g_brave_story_saved_cursor[0] = 1;
        } else {
            g_brave_story_saved_cursor[0] = 0;
        }
        brave_story_push_level();
        g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        menu_stack_close_parent_list_windows();
        message_block_load_if_changed(MESSAGE_BLOCK_STORY_EVENTS);
        file_wait_for_pending();
        g_brave_story_event_list_cursor = g_brave_story_saved_list_cursor_state;
        break;
    case MENU_STACK_LEVEL_TUTORIAL:
        window_set_render_objects_visible(0);
        audio_queue_wait_until_empty();
        audio_queue_start_music(AUDIO_MUSIC(2, MUSIC_TUTORIAL));
        break;
    case MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST:
        tutorial_build_menu_panel(&g_menu_stack_records[g_menu_stack_depth - 1].list_window);
        menu_stack_close_parent_list_frame();
        break;
    case MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST:
        window_set_render_objects_visible(0);
        screen_set_entry_4_cursor(3);
        world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth - 1]);
        tutorial_push_menu_level(1, 0);
        menu_stack_close_parent_list_frame();
        g_tutorial_list_cursor = g_brave_story_saved_list_cursor_state;
        break;
    case MENU_STACK_LEVEL_BRAVE_STORY_TREASURE:
        menu_stack_close_parent_list_windows();
        message_block_load_if_changed(MESSAGE_BLOCK_TREASURES);
        file_wait_for_pending();
        audio_queue_wait_until_empty();
        suzuki_sound_set_master_volume(0x3FFF, 1);
        audio_queue_start_music(AUDIO_MUSIC(2, MUSIC_BRAVE_STORY));
        break;
    default:
        break;
    }

    /* Blank the backdrop image from the end. */
    i = WORLD_MAP_BACKDROP_WORDS - 1;
    clear = &g_world_map_scratch_buffer[WORLD_MAP_BACKDROP_WORDS - 1];
    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].screen_transition.screen = screen;
    g_menu_stack_records[depth].screen_transition.param_a = param_a;
    g_menu_stack_records[depth].screen_transition.param_b = param_b;
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }

    screen_set_brightness(0, 0, 0);
    thread_start(WORLD_MAP_THREAD_SLOT_TRANSITION, formation_zodiac_backdrop_thread);
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_TRANSITION, 0, 0, 0);
    g_menu_stack_records[g_menu_stack_depth].screen_transition.delay = 0x14;
    g_menu_stack_records[g_menu_stack_depth].screen_transition.phase = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_SCREEN_TRANSITION;
    g_menu_stack_depth++;
}

/* Closes every window of the list-window level beneath the current one: the
 * main window goes to palette 10 (sequence, frame_index and anim_counter
 * cleared), the side window to palette 6, the frame and content render
 * records to palette 2, and the upper/lower windows to palette 5, each hidden. */
static void menu_stack_close_parent_list_windows(void) {
    s32 index = g_menu_stack_depth - 1;

    g_window_records[g_menu_stack_records[index].list_window.main_window].palette = 10;
    g_window_records[g_menu_stack_records[index].list_window.main_window].sequence = WINDOW_ANIM_CURSOR_STILL;
    g_window_records[g_menu_stack_records[index].list_window.main_window].frame_index
        = g_window_records[g_menu_stack_records[index].list_window.main_window].anim_counter = 0;
    g_window_records[g_menu_stack_records[index].list_window.main_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[index].list_window.side_window].palette = 6;
    g_window_records[g_menu_stack_records[index].list_window.side_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[index].list_window.frame_render].palette = 2;
    g_window_gs_image_records[g_menu_stack_records[index].list_window.frame_render].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[index].list_window.content_render].palette = 2;
    g_window_gs_image_records[g_menu_stack_records[index].list_window.content_render].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[index].list_window.upper_window].palette = 5;
    g_window_records[g_menu_stack_records[index].list_window.upper_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[index].list_window.lower_window].palette = 5;
    g_window_records[g_menu_stack_records[index].list_window.lower_window].flags |= DISPLAY_OBJECT_HIDDEN;
}

/* Closes the list window of the level beneath the current one: the main
 * window goes to palette 10 (sequence, frame_index and anim_counter cleared),
 * the side window to palette 6 and the frame render record to palette 2, each
 * hidden. */
static void menu_stack_close_parent_list_frame(void) {
    s32 index = g_menu_stack_depth - 1;

    g_window_records[g_menu_stack_records[index].list_window.main_window].palette = 10;
    g_window_records[g_menu_stack_records[index].list_window.main_window].sequence = WINDOW_ANIM_CURSOR_STILL;
    g_window_records[g_menu_stack_records[index].list_window.main_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[index].list_window.main_window].frame_index
        = g_window_records[g_menu_stack_records[index].list_window.main_window].anim_counter = 0;
    g_window_records[g_menu_stack_records[index].list_window.side_window].palette = 6;
    g_window_records[g_menu_stack_records[index].list_window.side_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[index].list_window.frame_render].palette = 2;
    g_window_gs_image_records[g_menu_stack_records[index].list_window.frame_render].flags |= DISPLAY_OBJECT_HIDDEN;
}

/*
 * Steps the menu level that swaps the world-map screen for a full-screen
 * menu level.
 *
 * After `delay` frames it stores the 256x240 frame buffer into the scratch
 * buffer, asks the transition thread to finish and forgets the loaded
 * background set. Once that thread has ended it pops itself, restarts the
 * message box (TEXT_ID_BRAVE_STORY_INTRO, or
 * TEXT_ID_TUTORIAL_WELCOME for the tutorial lists) and pushes the
 * level named by `screen`: Brave Story, a story event text, the tutorial
 * categories, the formation tutorials, the battle tutorials or a
 * treasure detail. The two tutorial entry lists first un-hide the parent
 * list's windows and frame.
 */
void screen_transition_step_level(menu_stack_record_t* level) {
    RECT rect;

    if (level->screen_transition.delay == 0) {
        if (level->screen_transition.phase == 0) {
            DrawSync(0);
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
            StoreImage(&rect, g_world_map_scratch_buffer);
            DrawSync(0);
            VSync(0);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_TRANSITION, 0, 0, 1);
            g_picture_background_loaded_set = -1;
            level->screen_transition.phase++;
            return;
        }
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_TRANSITION) != 0) {
            return;
        }
        g_menu_stack_depth--;
        switch (level->screen_transition.screen) {
        case MENU_STACK_LEVEL_BRAVE_STORY:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
            brave_story_push_level();
            break;
        case MENU_STACK_LEVEL_BRAVE_STORY_EVENTS:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
            brave_story_push_event_level(g_brave_story_replay_story_event_index);
            break;
        case MENU_STACK_LEVEL_TUTORIAL:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
            tutorial_push_menu_level(level->screen_transition.param_a, level->screen_transition.param_b);
            break;
        case MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
            g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.main_window].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.side_window].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.frame_render].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            tutorial_open_formation_list();
            break;
        case MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
            g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.main_window].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.side_window].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.frame_render].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            tutorial_open_battle_list();
            break;
        case MENU_STACK_LEVEL_BRAVE_STORY_TREASURE:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
            brave_story_push_treasure_level(level->screen_transition.param_a);
            break;
        }
    } else {
        level->screen_transition.delay--;
    }
}

/* Pushes the tutorial category menu (MENU_STACK_LEVEL_TUTORIAL)
 * with the cursor on selected_entry.
 *
 * Appends a cursor window, a header window and the frame render record, seeds
 * the four category entries, and positions both windows relative to the render
 * record's origin. With `reset` set it also clears the two 32-bit tutorial
 * progress masks at g_tutorial_progress_masks.
 *
 * The 40 bytes of `dead_locals` reproduce the target's 0x50 frame: the
 * original declares locals it never reads, and GCC still reserves their slots.
 * The `- -(...)` in the cursor-row Y expression only flips the commutative
 * operand order of the final `addu`; it is arithmetically identical. */
static void tutorial_push_menu_level(s32 selected_entry, s32 reset) {
    s32 dead_locals[10];
    s32 window_index;
    s32 record_index;

    if (reset != 0) {
        g_tutorial_progress_masks[1] = 0;
        g_tutorial_progress_masks[0] = 0;
    }

    window_index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].list_window.main_window = window_index;
    g_window_records[window_index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[window_index].priority = 0xA;
    g_window_records[window_index].anim_counter = 0;
    g_window_records[window_index].frame_index = 0;

    window_index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].list_window.side_window = window_index;
    g_window_records[window_index].sequence = WINDOW_ANIM_LIST_SIDE_LABEL;
    g_window_records[window_index].priority = 0xA;
    g_window_records[window_index].anim_counter = 0;
    g_window_records[window_index].frame_index = 0;
    g_menu_stack_records[g_menu_stack_depth].list_window.entries[0] = TUTORIAL_MENU_ROW_FORMATION;
    g_menu_stack_records[g_menu_stack_depth].list_window.entries[1] = TUTORIAL_MENU_ROW_BATTLE;
    g_menu_stack_records[g_menu_stack_depth].list_window.entries[2] = TUTORIAL_MENU_ROW_TOPICS;
    g_menu_stack_records[g_menu_stack_depth].list_window.entries[3] = TUTORIAL_MENU_ROW_LEAVE;
    g_menu_stack_records[g_menu_stack_depth].list_window.entry_count = 4;

    g_menu_stack_records[g_menu_stack_depth].list_window.frame_render
        = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    tutorial_build_menu_panel(&g_menu_stack_records[g_menu_stack_depth].list_window);

    record_index = g_menu_stack_depth;
    g_menu_stack_records[record_index].list_window.selected_entry = selected_entry;
    g_menu_stack_records[record_index].list_window.mode = MENU_STACK_WINDOW_MODE_BROWSE;
    g_window_records[g_menu_stack_records[record_index].list_window.main_window].position.x
        = g_window_gs_image_records[g_menu_stack_records[record_index].list_window.frame_render].base.x + 6;
    g_window_records[g_menu_stack_records[record_index].list_window.main_window].position.y
        = g_window_gs_image_records[g_menu_stack_records[record_index].list_window.frame_render].base.y
        - -(g_menu_stack_records[record_index].list_window.selected_entry * 0x10) + 0xE;
    g_window_records[g_menu_stack_records[record_index].list_window.side_window].position.x
        = g_window_gs_image_records[g_menu_stack_records[record_index].list_window.frame_render].base.x + 3;
    g_menu_stack_depth = record_index + 1;
    g_window_records[g_menu_stack_records[record_index].list_window.side_window].position.y
        = g_window_gs_image_records[g_menu_stack_records[record_index].list_window.frame_render].base.y - 2;
    g_menu_stack_records[record_index].list_window.content_render = 0x18;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_TUTORIAL;
}

/* Build the image of the 0x44-wide tutorial category panel: one 16-pixel row
 * per row id (TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST + id), at
 * (-0x78, -0x28) with OT priority 0xa, popping in.
 *
 * The base coordinates are written through a point view of base_x/base_y; the
 * target addresses them from &base_x, not from the record start. */
static void tutorial_build_menu_panel(menu_stack_window_level_t* panel) {
    RECT bounds;
    point32_t point;
    s32 index = panel->frame_render;
    point32_t* base;
    s32 i;

    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x44;
    bounds.h = panel->entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 0xA;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_window_gs_image_records[index].base;
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
void tutorial_resume_menu_level(menu_stack_record_t* level) {
    s32 main_window;
    s32 side_window;
    s32 slot;
    /* The original frame reserves eight unused bytes. */
    s32 pad[2];

    main_window = level->list_window.main_window;
    side_window = level->list_window.side_window;
    g_window_gs_image_records[level->list_window.frame_render].palette = 0;
    g_window_records[side_window].palette = 0;
    g_window_records[main_window].palette = 0;
    slot = level->list_window.main_window;
    g_window_records[slot].sequence = WINDOW_ANIM_CURSOR_IDLE;
    slot = level->list_window.main_window;
    g_window_records[slot].anim_counter = 0;
    g_window_records[slot].frame_index = 0;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
}

/* Gray out the tutorial category menu while a sub-level opens over it. The
 * target contains a second copy of bar_suspend_level; see there for
 * the matching notes. */
void tutorial_suspend_menu_level(menu_stack_record_t* level) {
    s32 index;

    g_window_records[level->list_window.main_window].palette = 10;
    g_window_records[level->list_window.side_window].palette = 6;
    g_window_gs_image_records[level->list_window.frame_render].palette = 2;
    g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
    index = level->list_window.main_window;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
}

/* Input step of the tutorial category menu: Circle opens the formation tutorials, the battle
 * tutorials or the topics (or leaves); Cross or Leave shows the parting prompt and fades out, back
 * to the title when opened from there; Select shows the row's help; Up and Down wrap. */
void tutorial_handle_menu_input(menu_stack_record_t* level) {
    u32 buttons;
    s32* selected_entry;

    if (g_window_gs_image_records[level->list_window.frame_render].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) {
        if (level->list_window.mode == MENU_STACK_WINDOW_MODE_FADE_OR_HELP && !(g_system_flags & SYSTEM_FLAG_FADING)) {
            level->list_window.mode = MENU_STACK_WINDOW_MODE_LEAVE_WAIT;
            world_map_clear_vram_and_scratch(1);
        }
        if (level->list_window.mode == MENU_STACK_WINDOW_MODE_LEAVE_WAIT
            && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            if ((g_system_flags & SYSTEM_FLAG_TITLE_TUTORIAL) == 0 || g_audio_queue.count == 0) {
                g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
                window_set_render_objects_visible(1);
                g_menu_stack_depth = g_menu_stack_depth - 1;
                g_system_flags = g_system_flags
                    & ~(SYSTEM_FLAG_HIDE_CONTEXT_VALUE | SYSTEM_FLAG_HIDE_CALENDAR_DATE
                        | SYSTEM_FLAG_HIDE_LOCATION_LABEL);
                menu_stack_dispatch_suspend();
                if (g_system_flags & SYSTEM_FLAG_TITLE_TUTORIAL) {
                    g_system_flags = (g_system_flags ^ (SYSTEM_FLAG_RUNNING | SYSTEM_FLAG_TITLE_TUTORIAL))
                        | SYSTEM_FLAG_RETURN_TO_TITLE;
                    return;
                }
                world_map_fade_start(WORLD_MAP_FADE_REDRAW_MAP, 0x10);
                world_map_push_menu_level();
            }
        }
        return;
    }

    if (level->list_window.mode == MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE) {
        if (g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            level->list_window.mode = MENU_STACK_WINDOW_MODE_FADE_OR_HELP;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
            g_system_flags = g_system_flags | SYSTEM_FLAG_INPUT_BLOCKED;
            world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
            if (g_system_flags & SYSTEM_FLAG_TITLE_TUTORIAL) {
                audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
                audio_queue_add(AUDIO_COMMAND_UNLOAD, 0);
            } else {
                audio_queue_start_music(AUDIO_MUSIC(1, MUSIC_WORLD_MAP));
            }
            g_tutorial_menu_saved_cursor = 0;
        }
        return;
    }

    if (g_audio_queue.count != 0) {
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_PROMPT, 0);
        level->list_window.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
        g_window_record_count -= 2;
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 3;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        tutorial_suspend_menu_level(level);
        menu_stack_push_message_level(level->list_window.entries[level->list_window.selected_entry] + 0x108B, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        selected_entry = &level->list_window.selected_entry;
        g_tutorial_menu_saved_cursor = *selected_entry;
        switch (level->list_window.entries[*selected_entry]) {
        case TUTORIAL_MENU_ROW_FORMATION:
            tutorial_suspend_menu_level(level);
            g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            tutorial_open_formation_list();
            return;
        case TUTORIAL_MENU_ROW_BATTLE:
            tutorial_suspend_menu_level(level);
            g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            tutorial_open_battle_list();
            return;
        case TUTORIAL_MENU_ROW_TOPICS:
            message_block_load_if_changed(MESSAGE_BLOCK_TUTORIAL_TOPICS);
            tutorial_suspend_menu_level(level);
            g_window_records[level->list_window.main_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            tutorial_open_topic_list();
            return;
        case TUTORIAL_MENU_ROW_LEAVE:
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_PROMPT, 0);
            level->list_window.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
            g_window_record_count -= 2;
            g_window_render_record_count -= 1;
            g_window_render_object_count -= 3;
            return;
        }
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (level->list_window.selected_entry != 0) {
            g_window_records[level->list_window.main_window].position.y -= 0x10;
            level->list_window.selected_entry--;
        } else {
            g_window_records[level->list_window.main_window].position.y += (level->list_window.entry_count - 1) * 0x10;
            level->list_window.selected_entry = level->list_window.entry_count - 1;
        }
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        if (level->list_window.selected_entry < level->list_window.entry_count - 1) {
            g_window_records[level->list_window.main_window].position.y += 0x10;
            level->list_window.selected_entry++;
        } else {
            g_window_records[level->list_window.main_window].position.y -= (level->list_window.entry_count - 1) * 0x10;
            level->list_window.selected_entry = 0;
        }
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }
}

/* Opens the list of the seven formation tutorials (TEXT_ID_TUTORIAL_NAME_BASE
 * + row), a row drawn in text color 8 once its bit of
 * g_tutorial_progress_masks[0] shows it was viewed, at (0x54, 0x50),
 * 0x8c wide, and pushes MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST.
 *
 * The flag value lives in its own variable so the target keeps 8 in a saved
 * register across the loop. */
static void tutorial_open_formation_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 i = 0;
    s32 count;
    s32 record_index;
    s32 enabled = 8;

    for (; i < 7; i++) {
        g_menu_stack_entry_values[i] = i + TEXT_ID_TUTORIAL_NAME_BASE;
        if (bits_test(g_tutorial_progress_masks, i) != 0) {
            g_menu_stack_row_flags[i] = enabled;
        } else {
            g_menu_stack_row_flags[i] = 0;
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
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
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
void tutorial_restore_formation_list_buttons(menu_stack_record_t* unused_level) {
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
void tutorial_handle_formation_list_input(menu_stack_record_t* level) {
    if (level->panel.result == 2) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            s16* selected = &g_window_panel_render_state.selected_index;

            bits_set_value(g_tutorial_progress_masks, *selected, 1);
            screen_set_brightness(0x80, 0x80, 0x80);
            formation_run_at_menu(*selected);
            g_menu_stack_ot_offset = 1;
            g_menu_blocked_buttons = 0;
            g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED | SYSTEM_FLAG_DISPLAY_OFF;
            g_menu_stack_depth--;
            screen_transition_push_level(MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST, 0, 0);
        }
        return;
    }

    if (g_system_flags & SYSTEM_FLAG_FADING) {
        return;
    }

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_FORMATION_TUTORIALS);
            level->panel.result = 2;
            g_system_flags |= SYSTEM_FLAG_DISPLAY_OFF;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
            return;
        }
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_FORMATION_TUTORIALS);
        menu_stack_dispatch_resume();
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        tutorial_block_formation_list_buttons(level);
        menu_stack_push_message_level(0x108B, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
    }
}

/* Opens the list of the eight battle tutorials (TEXT_ID_TUTORIAL_NAME_BASE +
 * 16 + row), a row drawn in text color 8 once its bit of
 * g_tutorial_progress_masks[1] shows it was viewed, at (0x54, 0x50),
 * 0x8c wide, and pushes MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST. Sibling of
 * tutorial_open_formation_list. */
static void tutorial_open_battle_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 i;
    s32 count;
    s32 record_index;

    for (i = 0; i < 8; i++) {
        g_menu_stack_entry_values[i] = i + TEXT_ID_TUTORIAL_NAME_BASE + 16;
        if (bits_test(&g_tutorial_progress_masks[1], i) != 0) {
            g_menu_stack_row_flags[i] = 8;
        } else {
            g_menu_stack_row_flags[i] = 0;
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
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
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
void tutorial_restore_battle_list_buttons(menu_stack_record_t* unused_level) {
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
 * SYSTEM_FLAG_TUTORIAL_EVENT. */
void tutorial_handle_battle_list_input(menu_stack_record_t* level) {
    s16* selected;

    if (level->panel.result == 2) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            selected = &g_window_panel_render_state.selected_index;
            bits_set_value(&g_tutorial_progress_masks[1], *selected, 1);
            script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_tutorial_battle_event_ids[*selected]);
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS);
            g_brave_story_saved_list_cursor_state = g_tutorial_list_cursor;
            g_system_flags = (g_system_flags | SYSTEM_FLAG_TUTORIAL_EVENT) ^ SYSTEM_FLAG_RUNNING;
            g_brave_story_saved_weather_variable = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        }
        return;
    }

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && !(g_system_flags & SYSTEM_FLAG_FADING)) {
        if (level->panel.result != 0) {
            g_system_flags |= SYSTEM_FLAG_DISPLAY_OFF;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
            level->panel.result = 2;
            return;
        }
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS);
        menu_stack_dispatch_resume();
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        tutorial_block_battle_list_buttons(level);
        menu_stack_push_message_level(0x108C, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
    }
}
