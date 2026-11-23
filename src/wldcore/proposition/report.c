/* Returned propositions play their participant reports, apply results and select the appropriate report messages. */

#include "fft/game/battle_unit.h"
#include "fft/game/job.h"
#include "fft/game/party.h"
#include "fft/game/party_unit.h"
#include "fft/gfx/display_object.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/portrait.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_screen.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/input.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "fft/world/bar.h"
#include "fft/world/brave_story.h"
#include "fft/world/calendar.h"
#include "fft/world/menu_stack.h"
#include "fft/world/menu_stack_records.h"
#include "fft/world/proposition.h"
#include "fft/world/window.h"
#include "fft/world/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Reward names in the TEXT_ID_PROPOSITION_REWARD_NAME_BASE section: the random bonuses from the base,
 * then the no-reward name, the unexplored lands and the treasures, each by reward_index. */
enum {
    TEXT_ID_REWARD_NONE = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 6,
    TEXT_ID_REWARD_LAND_FIRST = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x1b,
    TEXT_ID_REWARD_TREASURE_FIRST = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x2e
};

static void open_report(s32 argument);
static void open_result(s32 proposition);
static void prepare_report(s32 proposition_index);
static s32 select_report_speaker(s32 index);
static s32 update_report_message(proposition_report_level_t* state, s32 message);

/* Open the job report list: accepted jobs that have returned to this town, under 'Good job. You
 * came back from these jobs.'; with none, rebuild the bar rows and resume the bar. */
void proposition_open_returned_list(void) {
    point32_t origin;
    window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 total;
    s32 previous = g_menu_stack_depth - 1;
    s32 loaded_count;
    s32* location;
    s32 entry_value_base;

    if (g_display_object_gs_image_pool[g_menu_stack_records[previous].bar.rows_image].flags & DISPLAY_OBJECT_HIDDEN) {
        g_display_object_animated_pool[g_menu_stack_records[previous].bar.cursor_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[previous].bar.label_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[previous].bar.rows_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[previous].bar.rows_image].flags |= DISPLAY_OBJECT_POP_IN;
    }

    if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
    }

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_JOBS_RETURNED, 0);
    count = 0;
    loaded_count = g_proposition_count;
    i = 0;
    if (loaded_count > 0) {
        location = &g_world_map_marker_state.marker.location_id;
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        total = loaded_count;
        do {
            if ((g_proposition_active[i].flags & PROPOSITION_PROGRESS_RETURNED)
                && g_proposition_active[i].location == *location) {
                g_menu_stack_entry_values[count] = g_proposition_active[i].proposition_id + entry_value_base;
                g_menu_stack_row_text_colors[count] = 0;
                g_menu_stack_records[g_menu_stack_depth].panel.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }

    if (count != 0) {
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
        if (count < 6) {
            i = count;
        } else {
            i = 6;
        }
        layout.text_width = 0x96;
        origin.x = 0x48;
        layout.visible_rows = i;
        origin.y = 0x50;
        menu_stack_restore_cursor(MENU_STACK_CURSOR_SLOT_RETURNED, count, i);
        window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
            g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    } else {
        bar_build_rows(&g_menu_stack_records[g_menu_stack_depth - 1].bar);
        menu_stack_dispatch_resume();
        return;
    }

    g_menu_stack_ot_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_PROPOSITION_RETURNED_LIST;
    g_menu_stack_depth++;
}

/* Resume handler of the returned propositions list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_JOBS_RETURNED again. */
void proposition_resume_returned_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_JOBS_RETURNED, 0);
}

/* Withhold every button from the WORLD menu frame while a level opened from the job report list
 * runs. */
void proposition_suspend_returned_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Handle input on the job report list: Cross closes the list and Select shows
 * its help. Circle prepares the selected job's report, asks "Wanna hear about
 * ...?" and pushes the confirmation level with an OK/NO panel. */
void proposition_update_returned_list_input(menu_stack_record_t* level) {
    s32 buttons;
    s32 depth;
    point32_t origin;

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED)
        && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        menu_stack_dispatch_resume();
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_RETURNED);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        proposition_suspend_returned_list(level);
        menu_stack_open_message(0x1061, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        proposition_suspend_returned_list(level);
        prepare_report(level->panel.entries[g_window_panel_render_state.selected_index]);
        menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_RETURNED);
        g_text_substitution_values[0] = g_menu_stack_entry_values[g_window_panel_render_state.selected_index];
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_HEAR_ABOUT_JOB, 0);
        origin.x = 0x48;
        origin.y = 0x20;
        window_build_yes_no_panel(origin);
        depth = g_menu_stack_depth;
        g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_REPORT_CONFIRM;
        g_menu_stack_depth = depth + 1;
        g_menu_stack_records[depth].proposition_report_confirm.argument
            = level->panel.entries[g_window_panel_render_state.selected_index];
    }
}

/* Input step of the "Wanna hear about ...?" OK/NO panel over the job report
 * list: Up and Down toggle the row, moving the cursor window; Cross or NO
 * releases the panel and resumes the list; OK also closes the list's panel,
 * pops both levels and pushes the job's report
 * (open_report).
 *
 * The `- -(...)` in the cursor-row expression is load bearing, not a typo.
 * The target's final add is `addu v0,v0,v1` (base_y first, result tied to
 * base_y's register); every plain `+` spelling of the same sum compiles to
 * `addu v1,v1,v0`. Written with a `+`, expand hands combine a plus tree with a
 * constant in it, combine reassociates the 0xE onto the scaled selection and
 * rebuilds the outer add through gen_binary, which canonicalizes the complex
 * operand into position 0 -- so the sum is tied to the delta register and the
 * following `sw` reads it. Negating the addend makes the outer operation a
 * MINUS of a NEG, which combine turns back into a plus *after* that
 * canonicalization, leaving base_y as operand 0; the rest of the block
 * (`sll` at +0x120, address materialization, `lw` at +0x130, `addiu` filling
 * the load shadow at +0x134) is unchanged. `base_y - (0 - (selection * 0x10 +
 * 0xE))` and `base_y - (0 - (0xE + selection * 0x10))` are byte-identical
 * spellings; `base_y - (-0xE - selection * 0x10)` is not (fold collapses it
 * back to a plus tree and the tie returns). */
void proposition_update_report_confirm(menu_stack_record_t* level) {
    s32 selection;

    if (g_display_object_gs_image_pool[level->proposition_report_confirm.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
    block_tail:
        g_display_object_animated_count -= 2;
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 3;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        return;
    }
    if (input_update_repeat(PSX_PAD_UP) != 0 || input_update_repeat(PSX_PAD_DOWN) != 0) {
        selection = level->proposition_report_confirm.selection ^ 1;
        level->proposition_report_confirm.selection = selection;
        g_display_object_animated_pool[level->proposition_report_confirm.cursor_window].position.y
            = g_display_object_gs_image_pool[level->proposition_report_confirm.render_index].position.y
            - -(selection * 0x10 + 0xE);
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        if (level->proposition_report_confirm.selection != 0) {
            /* Shared tail: a duplicated release path is cross-jumped into
             * this arm instead of the cancel arm the target keeps. */
            goto block_tail;
        } else {
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
            g_display_object_animated_count -= 2;
            g_display_object_image_count -= 1;
            g_display_object_queue_count -= 3;
            g_menu_stack_depth -= 2;
            open_report(level->proposition_report_confirm.argument);
        }
    }
}

/* Hides the bar menu's windows and pushes MENU_STACK_LEVEL_PROPOSITION_REPORT
 * for active proposition `argument`, starting the report music, or in state 99
 * (no report) for a recalled job; then closes the message box.
 */
static void open_report(s32 argument) {
    s32 depth = g_menu_stack_depth;

    g_display_object_animated_pool[g_menu_stack_records[depth - 1].bar.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].bar.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[depth - 1].bar.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_records[depth].proposition_report.state = -1;
    g_menu_stack_records[depth].proposition_report.proposition = argument;
    g_menu_stack_records[depth].proposition_report.phase = 0;
    g_menu_stack_records[depth].proposition_report.load_phase = PROPOSITION_REPORT_LOAD_MESSAGES;
    g_menu_stack_records[depth].proposition_report.render_index = -1;
    g_menu_stack_ot_offset = 9;
    if (g_proposition_report.recalled != 0) {
        g_menu_stack_records[depth].proposition_report.state = 0x63;
    } else {
        music_queue_start_track(MUSIC_QUEUE_TRACK(2, MUSIC_PROPOSITION_REPORT));
    }
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_PROPOSITION_REPORT;
    g_menu_stack_depth++;
}

/*
 * Steps the proposition report level: the bar's account of a finished job.
 *
 * `load_phase` first loads the proposition's report message block and then,
 * for a treasure or unexplored-land reward, its picture set. `state` then
 * walks the report one acknowledged message at a time (each message id is the
 * first of a group of six, see update_report_message):
 *   0..3    the opening lines and a participant's line for the result;
 *   4..6    praise for the unit proposition_find_preferred_job_unit
 *           names, or 7..8 when there is none;
 *   9..10   the bad-failure lines, which skip the reward;
 *   11      branches on success to 12..14 (a random participant is named as
 *           the finder, and a treasure or land picture pops in beside its
 *           caption window with the completion sound) or to 15;
 *   16..17  the closing lines for the result, after which the two picture
 *           records are dropped and the message thread is told to close;
 *   18      waits for the message thread; the next step pops this level and
 *           pushes the result level.
 * The level is pushed in state -1, or in state 99 to skip the report. Both
 * share the wait arm of 18: -1 then continues at 0 and 99 goes straight to
 * the final step.
 */
void proposition_update_report(menu_stack_record_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut;
    s32 record;
    s32 count;
    s32 participant;
    s32 load_phase;
    s32* kind;
    s32* picture;

    /* The message states below run only after the message block has finished loading. */
    load_phase = level->proposition_report.load_phase;
    switch (load_phase) {
    case PROPOSITION_REPORT_LOAD_MESSAGES:
        if (g_music_queue.count != 0) {
            return;
        }
        level->proposition_report.load_phase = PROPOSITION_REPORT_LOAD_PICTURE;
        message_load_block_if_changed(g_proposition_active[level->proposition_report.proposition].proposition_id
            + MESSAGE_BLOCK_PROPOSITION_REPORT_FIRST);
        return;

    case PROPOSITION_REPORT_LOAD_PICTURE:
        if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
            return;
        }
        kind = &g_proposition_report.reward_type;
        if (*kind == PROPOSITION_REWARD_TREASURE) {
            world_map_load_picture_set(g_treasure_picture_sets[g_proposition_report.reward_index]);
        }
        if (*kind == PROPOSITION_REWARD_UNEXPLORED_LAND) {
            world_map_load_picture_set(g_unexplored_land_picture_sets[g_proposition_report.reward_index]);
        }
        level->proposition_report.load_phase = PROPOSITION_REPORT_LOAD_DONE;
        break;
    }

    switch (level->proposition_report.state) {
    case 0:
        if (update_report_message(&level->proposition_report, 0x36) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 1:
        if (update_report_message(&level->proposition_report, 0x3C) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 2:
        if (proposition_update_participant_message(&level->proposition_report, g_proposition_report.result * 6, -1)
            == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 3:
        if (update_report_message(&level->proposition_report, 0x42) == 0) {
            return;
        }
        if (g_proposition_report.result == PROPOSITION_RESULT_BAD_FAILURE) {
            level->proposition_report.state = 9;
            return;
        }

        /* The finder returns the party index + 1, or 0 when no participant qualifies. */
        count = proposition_find_preferred_job_unit();
        g_proposition_report.preferred_unit = count;
        if (count != 0) {
            g_proposition_report.preferred_unit = count - 1;
            g_text_substitution_values[1] = party_get_roster_unit(count - 1)->job_id + TEXT_ID_JOB_NAME_BASE;
            level->proposition_report.state = 4;
            return;
        }
        level->proposition_report.state = 7;
        return;

    case 4:
        if (update_report_message(&level->proposition_report, 0x48) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 5:
        if (update_report_message(&level->proposition_report, 0x4E) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 6:
        if (proposition_update_participant_message(
                &level->proposition_report, g_proposition_report.result * 6 + 0x12, g_proposition_report.preferred_unit)
            == 0) {
            return;
        }
        level->proposition_report.state = 0xB;
        return;

    case 7:
        if (update_report_message(&level->proposition_report, 0x54) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 8:
        if (update_report_message(&level->proposition_report, 0x5A) == 0) {
            return;
        }
        level->proposition_report.state = 0xB;
        return;

    case 9:
        if (update_report_message(&level->proposition_report, 0x60) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 10:
        if (update_report_message(&level->proposition_report, 0x66) == 0) {
            return;
        }
        level->proposition_report.state = 0x10;
        return;

    case 11:
        if (update_report_message(&level->proposition_report, 0x6C) == 0) {
            return;
        }
        if (g_proposition_report.result == PROPOSITION_RESULT_SUCCESS) {
            level->proposition_report.state = 0xC;
            return;
        }
        level->proposition_report.state = 0xF;
        return;

    case 12:
        if (update_report_message(&level->proposition_report, 0x72) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 13:
        /* Name a random participant as the finder; rand() is 15 bits, so the >> 15 gives a slot below count. */
        if (level->proposition_report.phase == 0) {
            participant
                = g_proposition_active[g_proposition_report.proposition_index].participant_indices
                      [(rand() * g_proposition_active[g_proposition_report.proposition_index].participant_count) >> 15];
            if (participant == g_proposition_report.reporter_party_index) {
                g_text_substitution_values[1] = proposition_adjust_message(0xE5, participant) + TEXT_ID_WORLD_MENU_BASE;
            } else {
                g_text_substitution_values[1] = participant + TEXT_ID_UNIT_NAME_BASE;
            }
        }
        if (update_report_message(&level->proposition_report, 0x78) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 14:
        /* Wait for the picture set the load phase requested. */
        if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
            return;
        }

        /* On the message's first call, add the picture and its caption window. */
        if (level->proposition_report.phase == 0) {
            picture = &g_proposition_report.reward_type;
            if (*picture == PROPOSITION_REWARD_TREASURE) {
                record = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
                level->proposition_report.render_index = record;
                GsGetTimInfo(g_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.x = image.px;
                bounds.y = image.py;
                bounds.w = image.pw;
                bounds.h = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                display_object_init_gs_image(record, bounds, clut, DISPLAY_OBJECT_8BIT_CLUT);
                g_display_object_gs_image_pool[record].priority = 9;
                g_display_object_gs_image_pool[record].flags |= DISPLAY_OBJECT_POP_IN;
                {
                    point32_t* base = &g_display_object_gs_image_pool[record].position;
                    base->x = -image.pw - 4;
                    base->y = -(image.ph >> 1) + 0x1A;
                }

                record = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
                bounds.x = 0;
                bounds.y = 0x70;
                bounds.w = 0x8C;
                bounds.h = 0x8C;
                window_init_text_record(record, bounds, 0, 0, g_window_image_buffer);
                g_display_object_gs_image_pool[record].priority = 9;
                {
                    point32_t* base = &g_display_object_gs_image_pool[record].position;
                    base->x = -0x46;
                    base->y = -0x2A;
                }
                sound_play_effect(SOUND_SFX_COMPLETE);
            }
            if (*picture == PROPOSITION_REWARD_UNEXPLORED_LAND) {
                record = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
                level->proposition_report.render_index = record;
                GsGetTimInfo(g_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.x = image.px;
                bounds.y = image.py;
                bounds.w = image.pw;
                bounds.h = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                display_object_init_gs_image(record, bounds, clut, DISPLAY_OBJECT_8BIT_CLUT);
                g_display_object_gs_image_pool[record].priority = 9;
                g_display_object_gs_image_pool[record].flags |= DISPLAY_OBJECT_POP_IN;
                {
                    point32_t* base = &g_display_object_gs_image_pool[record].position;
                    base->x = -image.pw;
                    base->y = -(image.ph >> 1);
                }

                record = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
                bounds.x = 0;
                bounds.y = 0x80;
                bounds.w = 0x88;
                bounds.h = 0x5A;
                window_init_text_record(record, bounds, 0, 0, g_window_image_buffer);
                g_display_object_gs_image_pool[record].priority = 9;
                {
                    point32_t* base = &g_display_object_gs_image_pool[record].position;
                    base->x = -0x41;
                    base->y = -0x2C;
                }
                sound_play_effect(SOUND_SFX_COMPLETE);
            }
        }
        if (update_report_message(&level->proposition_report, 0x7E) == 0) {
            return;
        }
        level->proposition_report.state = 0x10;
        return;

    case 15:
        if (update_report_message(&level->proposition_report, 0x84) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 16:
        if (update_report_message(&level->proposition_report, g_proposition_report.result * 6 + 0x8A) == 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    case 17:
        if (proposition_update_participant_message(
                &level->proposition_report, g_proposition_report.result * 6 + 0x24, -1)
            == 0) {
            return;
        }
        level->proposition_report.state++;

        /* Drop the picture and caption records that state 14 appended. */
        if (g_proposition_report.reward_type == PROPOSITION_REWARD_TREASURE) {
            g_display_object_image_count -= 2;
            g_display_object_queue_count -= 2;
        }
        if (g_proposition_report.reward_type == PROPOSITION_REWARD_UNEXPLORED_LAND) {
            g_display_object_image_count -= 2;
            g_display_object_queue_count -= 2;
        }
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        return;

    case -1:
    case 18:
    case 99:
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        level->proposition_report.state++;
        return;

    default:
        g_menu_stack_ot_offset = 1;
        g_menu_stack_depth--;
        open_result(level->proposition_report.proposition);
        return;
    }
}

/* Build the proposition-result panel, total its gil reward, start the reporting
 * unit's message, and push the result level.
 *
 * The first-column local stays live across the text calls. Reading the result
 * through the shared record keeps its load after the level-record stores. */
static void open_result(s32 proposition) {
    RECT bounds;
    point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 i;
    s32 message;
    s32 count = g_proposition_active[proposition].participant_count;
    s32 depth;
    u16* panel_text;
    s32* party;
    point32_t* base;
    s32 rows;

    g_proposition_gil_amount = 0;
    for (i = 0; i < count; i++) {
        g_proposition_gil_amount += g_proposition_report.participant_gil[i];
    }

    g_proposition_gil_amount += g_proposition_report.reward_value;
    g_text_substitution_values[1] = g_proposition_gil_amount;
    switch (g_proposition_report.reward_type) {
    case PROPOSITION_REWARD_TREASURE:
        g_text_substitution_values[0] = g_proposition_report.reward_index + TEXT_ID_REWARD_TREASURE_FIRST;
        break;
    case PROPOSITION_REWARD_UNEXPLORED_LAND:
        g_text_substitution_values[0] = g_proposition_report.reward_index + TEXT_ID_REWARD_LAND_FIRST;
        break;
    case PROPOSITION_REWARD_RANDOM_BONUS:
        g_text_substitution_values[0] = g_proposition_report.reward_index + TEXT_ID_PROPOSITION_REWARD_NAME_BASE;
        break;
    default:
        g_text_substitution_values[0] = TEXT_ID_REWARD_NONE;
        break;
    }

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    panel_text = &g_proposition_selected_row.name_text;
    g_menu_stack_records[g_menu_stack_depth].proposition_result.render_index = index;
    text_measure_pixels(
        &text_width, &text_rows, text_find_entry(*panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    bounds.x = 0;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    rows = text_rows + 2;
    bounds.h = (count + rows) * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    window_display_text_entry(index, TEXT_ID_JOB_RESULT_LABELS, point, g_window_image_buffer);
    point.x = 8;
    point.y = 8;
    window_display_text_entry(index, TEXT_ID_JOB_CONTENTS_LABEL, point, g_window_image_buffer);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry(
        index, *panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point, g_window_image_buffer);
    for (i = 0; i < count; i++) {
        s32 row = i + 2;
        s32 x = 0x38;

        point.y = (text_rows + row) * 0x10 + 8;
        point.x = x;
        window_display_text_entry(index,
            g_proposition_active[proposition].participant_indices[i] | TEXT_ID_UNIT_NAME_BASE, point,
            g_window_image_buffer);
    }

    for (i = 0; i < count; i++) {
        s32 row = i + 2;

        point.x = 0xA0;
        point.y = (text_rows + row) * 0x10 + 8;
        g_text_substitution_values[0] = g_proposition_report.participant_jp[i];
        window_display_text_entry(index, TEXT_ID_FOUR_DIGIT_VALUE, point, g_window_image_buffer);
    }

    window_load_render_record_image(index, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 9;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_display_object_gs_image_pool[index].position;
    base->x = -0x7A;
    base->y = -0x2A;
    if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
    }

    party = &g_proposition_report.reporter_party_index;
    g_text_substitution_values[0] = *party + TEXT_ID_UNIT_NAME_BASE;
    message = 0x9C;
    script_variable_set(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, portrait_get_party_unit_code(*party));
    if (g_proposition_report.recalled != 0) {
        message = 0xA2;
    }

    if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
    }

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19,
        proposition_adjust_message(message, *party) + TEXT_ID_WORLD_MESSAGE_BASE, 0);
    g_menu_stack_records[g_menu_stack_depth].proposition_result.proposition = proposition;
    g_menu_stack_records[g_menu_stack_depth].proposition_result.delay = 0;
    switch (g_proposition_report.result) {
    case PROPOSITION_RESULT_SUCCESS:
        suzuki_sound_play_2_sfx(SOUND_SFX_PROPOSITION_SUCCESS_A, SOUND_SFX_PROPOSITION_SUCCESS_B);
        g_menu_stack_records[g_menu_stack_depth].proposition_result.delay = 0x78;
        break;
    case PROPOSITION_RESULT_FAILURE:
        suzuki_sound_play_2_sfx(SOUND_SFX_PROPOSITION_PARTIAL_A, SOUND_SFX_PROPOSITION_PARTIAL_B);
        break;
    case PROPOSITION_RESULT_BAD_FAILURE:
        sound_play_effect(SOUND_SFX_PROPOSITION_FAILURE);
        break;
    }

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].proposition_result.phase = PROPOSITION_RESULT_PHASE_INPUT;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_RESULT;
    g_menu_stack_depth = depth + 1;
}

/* Per-frame step of the proposition-result menu level.
 *
 * Phase 0 waits for confirm/cancel and then commits the result: it clears the
 * proposition variable's PROPOSITION_VARIABLE_IN_PROGRESS (and sets
 * PROPOSITION_VARIABLE_COMPLETED on success), adds the reward total
 * g_proposition_gil_amount to gil (saturating at 99,999,999), records
 * the completion date (on success only) and any treasure or land date in the
 * packed nine-bit arrays, releases each participant from the
 * proposition and pays them their share, marks the proposition
 * state byte, and finally compacts the participant record out of
 * g_proposition_active. CLOSING waits for the message box and the audio
 * queue and restarts the bar music; DONE pops the level and hands off to the
 * discovery level or to proposition_open_returned_list.
 *
 * The proposition, result and share 32-bit fields are read as g_proposition_report
 * members: the target keeps that block's address in a register, which GCC
 * only does for this aggregate view, not for separately bound scalar globals. */
void proposition_update_result(menu_stack_record_t* level) {
    calendar_date_t date;
    s32 proposition_flags;
    s32 war_funds;
    s32 day;
    s32 month;
    s32 i;

    if (g_display_object_gs_image_pool[level->proposition_result.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (level->proposition_result.phase == PROPOSITION_RESULT_PHASE_DONE) {
        if (g_music_queue.count == 0 && g_music_queue.current_command == MUSIC_QUEUE_COMMAND_NONE) {
            g_menu_stack_depth = g_menu_stack_depth - 1;
            if (proposition_get_milestone_rank() != 0) {
                proposition_open_discovery();
                return;
            }
            if (g_proposition_report.recalled == 0) {
                music_queue_add(MUSIC_QUEUE_COMMAND_FADE_IN, 0x10);
            }
            proposition_open_returned_list();
        }
        return;
    }

    if (level->proposition_result.phase == PROPOSITION_RESULT_PHASE_CLOSING) {
        if (g_music_queue.count == 0 && g_music_queue.current_command == MUSIC_QUEUE_COMMAND_NONE
            && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            if (proposition_get_milestone_rank() == 0 && g_proposition_report.recalled == 0) {
                music_queue_add(MUSIC_QUEUE_COMMAND_PLAY_TRACK, MUSIC_QUEUE_TRACK(2, MUSIC_BAR));
            }
            level->proposition_result.phase = PROPOSITION_RESULT_PHASE_DONE;
        }
        return;
    }

    if (level->proposition_result.delay != 0) {
        level->proposition_result.delay = level->proposition_result.delay - 1;
        if (level->proposition_result.delay == 0) {
            suzuki_sound_play_2_sfx(SOUND_SFX_PROPOSITION_SUCCESS_A, SOUND_SFX_PROPOSITION_SUCCESS_B);
        }
    }

    if (!(g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
        return;
    }

    g_display_object_image_count = g_display_object_image_count - 1;
    g_display_object_queue_count = g_display_object_queue_count - 1;
    /* 0xFFFD clears PROPOSITION_VARIABLE_IN_PROGRESS from the 16-bit value. */
    proposition_flags = script_variable_get(g_proposition_active[level->proposition_result.proposition].proposition_id
                            + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
        & 0xFFFD;
    if (g_proposition_report.result == PROPOSITION_RESULT_SUCCESS) {
        proposition_flags |= PROPOSITION_VARIABLE_COMPLETED;
    }

    script_variable_set(
        g_proposition_active[level->proposition_result.proposition].proposition_id + EVENT_SCRIPT_VAR_PROPOSITION_FIRST,
        proposition_flags);
    war_funds = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) + g_proposition_gil_amount;
    if (war_funds > EVENT_WAR_FUNDS_MAX) {
        war_funds = EVENT_WAR_FUNDS_MAX;
    }

    script_variable_set(EVENT_SCRIPT_VAR_WAR_FUNDS, war_funds);
    g_hud_value = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
    day = script_variable_get(EVENT_SCRIPT_VAR_DAY);
    month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
    if (g_proposition_report.result == PROPOSITION_RESULT_SUCCESS) {
        date.day = day;
        date.month = month;
        calendar_write_date(g_proposition_last_attempt_date_bits,
            g_proposition_active[level->proposition_result.proposition].proposition_id, &date);
    }

    if (g_proposition_report.reward_type == PROPOSITION_REWARD_TREASURE) {
        script_variable_set(g_proposition_report.reward_index + EVENT_SCRIPT_VAR_TREASURE_FIRST, 1);
        date.day = day;
        date.month = month;
        calendar_write_date(g_brave_story_treasure_acquisition_date_bits, g_proposition_report.reward_index, &date);
    }

    if (g_proposition_report.reward_type == PROPOSITION_REWARD_UNEXPLORED_LAND) {
        script_variable_set(g_proposition_report.reward_index + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST, 1);
        date.day = day;
        date.month = month;
        calendar_write_date(g_brave_story_land_discovery_date_bits, g_proposition_report.reward_index, &date);
    }

    for (i = 0; i < g_proposition_active[g_proposition_report.proposition_index].participant_count; i++) {
        party_get_roster_unit(g_proposition_active[g_proposition_report.proposition_index].participant_indices[i])
            ->proposition_status = 0;
        job_add_proposition_jp(g_proposition_active[g_proposition_report.proposition_index].participant_indices[i],
            g_proposition_report.participant_jp[i]);
    }

    /* A failed job returns to the bar after its cooldown. */
    if (g_proposition_report.result == PROPOSITION_RESULT_FAILURE) {
        g_proposition_states[g_proposition_active[g_proposition_report.proposition_index].proposition_id]
            = PROPOSITION_STATE_FAILURE | PROPOSITION_FAILURE_COOLDOWN_DAYS;
    }

    if (g_proposition_report.result == PROPOSITION_RESULT_BAD_FAILURE) {
        g_proposition_states[g_proposition_active[g_proposition_report.proposition_index].proposition_id]
            = PROPOSITION_STATE_BAD_FAILURE | PROPOSITION_BAD_FAILURE_COOLDOWN_DAYS;
    }

    for (i = level->proposition_result.proposition; i < g_proposition_count - 1; i++) {
        g_proposition_active[i] = g_proposition_active[i + 1];
    }

    g_proposition_count--;
    if (proposition_get_milestone_rank() == 0 && g_proposition_report.recalled == 0) {
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x10);
    }

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
    level->proposition_result.phase = PROPOSITION_RESULT_PHASE_CLOSING;
}

/* Resolve finished proposition `proposition_index` before its report: unpack its data
 * row, pick the unit who reports, decide the result and work out the JP and
 * gil, the bonus reward and the extra-days multiplier. */
static void prepare_report(s32 proposition_index) {
    proposition_unpack_row(&g_proposition_selected_row, g_proposition_active[proposition_index].proposition_id);
    g_proposition_report.proposition_index = proposition_index;
    g_proposition_report.reporter_party_index = select_report_speaker(proposition_index);
    proposition_determine_success();
    proposition_calculate_jp_and_gil();
    proposition_roll_bonus_reward();
    proposition_apply_extra_days();
}

/* Pick the participant who reports on a finished job: the only one, else the
 * one with the highest or lowest Brave or Faith, as the unpacked row's
 * speaker_mode says. A mode outside 1 to 4 would return `selected` unset. The
 * goto form for the shared Brave/Faith tails matches the target, which
 * reloads the stat after the compare. */
static s32 select_report_speaker(s32 index) {
    party_unit_t* unit;
    s32 best;
    s32 i;
    s32 selected;

    if (g_proposition_active[index].participant_count == 1) {
        return g_proposition_active[index].participant_indices[0];
    }
    best = (g_proposition_selected_row.speaker_mode == PROPOSITION_SPEAKER_HIGHEST_BRAVE
               || g_proposition_selected_row.speaker_mode == PROPOSITION_SPEAKER_HIGHEST_FAITH)
        ? -1
        : 99999;
    for (i = 0; i < g_proposition_active[index].participant_count; i++) {
        unit = party_get_roster_unit(g_proposition_active[index].participant_indices[i]);
        switch (g_proposition_selected_row.speaker_mode) {
        case PROPOSITION_SPEAKER_HIGHEST_BRAVE:
            if (best < unit->brave) {
                goto take_brave;
            }
            break;
        case PROPOSITION_SPEAKER_LOWEST_BRAVE:
            if (unit->brave < best) {
            take_brave:
                best = unit->brave;
                selected = g_proposition_active[index].participant_indices[i];
            }
            break;
        case PROPOSITION_SPEAKER_HIGHEST_FAITH:
            if (best < unit->faith) {
                goto take_faith;
            }
            break;
        case PROPOSITION_SPEAKER_LOWEST_FAITH:
            if (unit->faith < best) {
            take_faith:
                best = unit->faith;
                selected = g_proposition_active[index].participant_indices[i];
            }
            break;
        }
    }
    return selected;
}

/* Adjust message `base` for the unit who speaks it: add the entry of the 3x3
 * PROPOSITION_DATA_MESSAGE_ADJUSTMENTS table for the unit's Faith and
 * Brave bands (below 36, 66 and 101), and 3 more unless the unit is male.
 * A value of 101 or more would leave its band unset; stats never get there. */
s32 proposition_adjust_message(s32 base, s32 party_index) {
    s32 faith_band;
    s32 brave_band;
    u32 is_not_male;
    party_unit_t* party = party_get_roster_unit(party_index);
    const u8* adjustments;

    if (party->brave < 0x65) {
        brave_band = 2;
    }
    if (party->brave < 0x42) {
        brave_band = 1;
    }
    if (party->brave < 0x24) {
        brave_band = 0;
    }
    if (party->faith < 0x65) {
        faith_band = 2;
    }
    if (party->faith < 0x42) {
        faith_band = 1;
    }
    if (party->faith < 0x24) {
        faith_band = 0;
    }
    is_not_male = party->gender_flags & UNIT_FLAG_MALE;
    is_not_male = is_not_male < 1;
    adjustments = proposition_get_data(PROPOSITION_DATA_MESSAGE_ADJUSTMENTS);
    return base + adjustments[faith_band * 3 + brave_band] + is_not_male * 3;
}

/*
 * Queue a proposition message and wait for its acknowledgment.
 *
 * The first call shows message `message`, adjusted for the reporting unit and
 * with its portrait; return 1 and clear the caller's phase once Circle is
 * pressed with the message box ready. Until then return 0.
 */
static s32 update_report_message(proposition_report_level_t* state, s32 message) {
    if (state->phase == 0) {
        s32* party_index;
        state->phase = 1;
        if (!thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE))
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        party_index = &g_proposition_report.reporter_party_index;
        g_text_substitution_values[0] = *party_index + TEXT_ID_UNIT_NAME_BASE;
        script_variable_set(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, portrait_get_party_unit_code(*party_index));
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19,
            proposition_adjust_message(message, *party_index) + TEXT_ID_WORLD_MESSAGE_BASE, 0);
    } else if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        if (menu_screen_is_dialog_waiting()) {
            state->phase = 0;
            return 1;
        }
    }
    return 0;
}
