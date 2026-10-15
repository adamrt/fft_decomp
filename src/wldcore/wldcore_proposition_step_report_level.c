#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"

/*
 * Steps the proposition report level: the bar's account of a finished job.
 *
 * `load_phase` first loads the proposition's report message block and then,
 * for a treasure or unexplored-land reward, its picture set. `state` then
 * walks the report one acknowledged message at a time (each message id is the
 * first of a group of six, see wldcore_proposition_step_message):
 *   0..3    the opening lines and a participant's line for the result;
 *   4..6    praise for the unit wldcore_proposition_find_preferred_job_unit
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
void wldcore_proposition_step_report_level(wldcore_proposition_report_level_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut;
    s32 record;
    s32 count;
    s32 participant;
    s32 load_phase;
    s32* kind;
    s32* picture;

    load_phase = level->load_phase;
    switch (load_phase) {
    case WLDCORE_PROPOSITION_REPORT_LOAD_MESSAGES:
        if (g_wldcore_audio_queue.count != 0) {
            return;
        }
        level->load_phase = WLDCORE_PROPOSITION_REPORT_LOAD_PICTURE;
        wldcore_load_message_block_if_changed(g_main_active_propositions[level->proposition].proposition_id
            + WLDCORE_MESSAGE_BLOCK_PROPOSITION_REPORT_FIRST);
        return;
    case WLDCORE_PROPOSITION_REPORT_LOAD_PICTURE:
        if (g_main_file_still_loading != 0) {
            return;
        }
        kind = &g_wldcore_proposition_report.reward_type;
        if (*kind == WLDCORE_PROPOSITION_REWARD_TREASURE) {
            wldcore_proposition_load_picture_data_set(
                g_wldcore_treasure_picture_sets[g_wldcore_proposition_report.reward_index]);
        }
        if (*kind == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
            wldcore_proposition_load_picture_data_set(
                g_wldcore_unexplored_land_picture_sets[g_wldcore_proposition_report.reward_index]);
        }
        level->load_phase = WLDCORE_PROPOSITION_REPORT_LOAD_DONE;
        break;
    }

    switch (level->state) {
    case 0:
        if (wldcore_proposition_step_message(level, 0x36) == 0) {
            return;
        }
        level->state++;
        return;
    case 1:
        if (wldcore_proposition_step_message(level, 0x3C) == 0) {
            return;
        }
        level->state++;
        return;
    case 2:
        if (wldcore_proposition_step_participant_message(level, g_wldcore_proposition_report.result * 6, -1) == 0) {
            return;
        }
        level->state++;
        return;
    case 3:
        if (wldcore_proposition_step_message(level, 0x42) == 0) {
            return;
        }
        if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_BAD_FAILURE) {
            level->state = 9;
            return;
        }
        count = wldcore_proposition_find_preferred_job_unit();
        g_wldcore_proposition_report.preferred_unit = count;
        if (count != 0) {
            g_wldcore_proposition_report.preferred_unit = count - 1;
            g_text_substitution_values[1] = wldcore_get_party_unit(count - 1)->job_id + TEXT_ID_JOB_NAME_BASE;
            level->state = 4;
            return;
        }
        level->state = 7;
        return;
    case 4:
        if (wldcore_proposition_step_message(level, 0x48) == 0) {
            return;
        }
        level->state++;
        return;
    case 5:
        if (wldcore_proposition_step_message(level, 0x4E) == 0) {
            return;
        }
        level->state++;
        return;
    case 6:
        if (wldcore_proposition_step_participant_message(
                level, g_wldcore_proposition_report.result * 6 + 0x12, g_wldcore_proposition_report.preferred_unit)
            == 0) {
            return;
        }
        level->state = 0xB;
        return;
    case 7:
        if (wldcore_proposition_step_message(level, 0x54) == 0) {
            return;
        }
        level->state++;
        return;
    case 8:
        if (wldcore_proposition_step_message(level, 0x5A) == 0) {
            return;
        }
        level->state = 0xB;
        return;
    case 9:
        if (wldcore_proposition_step_message(level, 0x60) == 0) {
            return;
        }
        level->state++;
        return;
    case 10:
        if (wldcore_proposition_step_message(level, 0x66) == 0) {
            return;
        }
        level->state = 0x10;
        return;
    case 11:
        if (wldcore_proposition_step_message(level, 0x6C) == 0) {
            return;
        }
        if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_SUCCESS) {
            level->state = 0xC;
            return;
        }
        level->state = 0xF;
        return;
    case 12:
        if (wldcore_proposition_step_message(level, 0x72) == 0) {
            return;
        }
        level->state++;
        return;
    case 13:
        if (level->phase == 0) {
            participant = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices
                              [(rand()
                                   * g_main_active_propositions[g_wldcore_proposition_report.proposition_index]
                                       .participant_count)
                                  >> 15];
            if (participant == g_wldcore_proposition_report.excluded_party_index) {
                g_text_substitution_values[1]
                    = wldcore_proposition_adjust_message_index(0xE5, participant) + TEXT_ID_WORLD_MENU_BASE;
            } else {
                g_text_substitution_values[1] = participant + TEXT_ID_UNIT_NAME_BASE;
            }
        }
        if (wldcore_proposition_step_message(level, 0x78) == 0) {
            return;
        }
        level->state++;
        return;
    case 14:
        if (g_main_file_still_loading != 0) {
            return;
        }
        if (level->phase == 0) {
            picture = &g_wldcore_proposition_report.reward_type;
            if (*picture == WLDCORE_PROPOSITION_REWARD_TREASURE) {
                record = window_append_record_variant(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                level->render_index = record;
                GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.x = image.px;
                bounds.y = image.py;
                bounds.w = image.pw;
                bounds.h = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                wldcore_window_init_vram_render_record(record, bounds, clut, WORLD_RENDER_RECORD_8BIT_CLUT);
                g_wldcore_window_render_records[record].priority = 9;
                g_wldcore_window_render_records[record].flags |= WORLD_RENDER_RECORD_POP_IN;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -image.pw - 4;
                    base->y = -(image.ph >> 1) + 0x1A;
                }

                record = window_append_record_variant(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                bounds.x = 0;
                bounds.y = 0x70;
                bounds.w = 0x8C;
                bounds.h = 0x8C;
                window_init_text_record_variant(record, bounds, 0, 0, g_wldcore_window_image_buffer);
                g_wldcore_window_render_records[record].priority = 9;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -0x46;
                    base->y = -0x2A;
                }
                wldcore_sound_play_effect(MAIN_SFX_COMPLETE);
            }
            if (*picture == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
                record = window_append_record_variant(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                level->render_index = record;
                GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.x = image.px;
                bounds.y = image.py;
                bounds.w = image.pw;
                bounds.h = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                wldcore_window_init_vram_render_record(record, bounds, clut, WORLD_RENDER_RECORD_8BIT_CLUT);
                g_wldcore_window_render_records[record].priority = 9;
                g_wldcore_window_render_records[record].flags |= WORLD_RENDER_RECORD_POP_IN;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -image.pw;
                    base->y = -(image.ph >> 1);
                }

                record = window_append_record_variant(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                bounds.x = 0;
                bounds.y = 0x80;
                bounds.w = 0x88;
                bounds.h = 0x5A;
                window_init_text_record_variant(record, bounds, 0, 0, g_wldcore_window_image_buffer);
                g_wldcore_window_render_records[record].priority = 9;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -0x41;
                    base->y = -0x2C;
                }
                wldcore_sound_play_effect(MAIN_SFX_COMPLETE);
            }
        }
        if (wldcore_proposition_step_message(level, 0x7E) == 0) {
            return;
        }
        level->state = 0x10;
        return;
    case 15:
        if (wldcore_proposition_step_message(level, 0x84) == 0) {
            return;
        }
        level->state++;
        return;
    case 16:
        if (wldcore_proposition_step_message(level, g_wldcore_proposition_report.result * 6 + 0x8A) == 0) {
            return;
        }
        level->state++;
        return;
    case 17:
        if (wldcore_proposition_step_participant_message(level, g_wldcore_proposition_report.result * 6 + 0x24, -1)
            == 0) {
            return;
        }
        level->state++;
        if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 2;
        }
        if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 2;
        }
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        return;
    case -1:
    case 18:
    case 99:
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        level->state++;
        return;
    default:
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_stack_depth--;
        wldcore_proposition_push_result_level(level->proposition);
        return;
    }
}
