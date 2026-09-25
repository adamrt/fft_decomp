#include "fft/wldcore.h"
#include "psx/gpu.h"
#include "psx/gs.h"
#include "psx/libc.h"

void wldcore_proposition_step_report_level(wldcore_proposition_report_level_t* level) {
    GsIMAGE image;
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t clut;
    s32 record;
    s32 count;
    s32 participant;
    s32 load_phase;
    s32* kind;
    s32* picture;

    load_phase = level->load_phase;
    switch (load_phase) {
    case 1:
        if (g_wldcore_audio_queue.count != 0) {
            return;
        }
        level->load_phase = 2;
        wldcore_load_message_block_if_changed(g_main_active_propositions[level->proposition].proposition_id + 0x10);
        return;
    case 2:
        if (g_main_file_still_loading != 0) {
            return;
        }
        kind = &g_wldcore_job_selection.reward_type;
        if (*kind == 1) {
            wldcore_proposition_load_picture_data_set(
                g_wldcore_treasure_picture_sets[g_wldcore_job_selection.reward_index]);
        }
        if (*kind == 2) {
            wldcore_proposition_load_picture_data_set(
                g_wldcore_unexplored_land_picture_sets[g_wldcore_job_selection.reward_index]);
        }
        level->load_phase = 0;
        break;
    }

    switch (level->state) {
    case 0:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x36) == 0) {
            return;
        }
        level->state++;
        return;
    case 1:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x3C) == 0) {
            return;
        }
        level->state++;
        return;
    case 2:
        if (wldcore_proposition_step_participant_message(
                (wldcore_proposition_message_state_t*)level, g_wldcore_job_selection.result * 6, -1)
            == 0) {
            return;
        }
        level->state++;
        return;
    case 3:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x42) == 0) {
            return;
        }
        if (g_wldcore_job_selection.result == 2) {
            level->state = 9;
            return;
        }
        count = wldcore_proposition_find_preferred_job_unit();
        g_wldcore_job_selection.preferred_unit = count;
        if (count != 0) {
            g_wldcore_job_selection.preferred_unit = count - 1;
            g_world_text_substitution_values[1] = wldcore_get_party_data_pointer(count - 1)->job_id + 0x3000;
            level->state = 4;
            return;
        }
        level->state = 7;
        return;
    case 4:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x48) == 0) {
            return;
        }
        level->state++;
        return;
    case 5:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x4E) == 0) {
            return;
        }
        level->state++;
        return;
    case 6:
        if (wldcore_proposition_step_participant_message((wldcore_proposition_message_state_t*)level,
                g_wldcore_job_selection.result * 6 + 0x12, g_wldcore_job_selection.preferred_unit)
            == 0) {
            return;
        }
        level->state = 0xB;
        return;
    case 7:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x54) == 0) {
            return;
        }
        level->state++;
        return;
    case 8:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x5A) == 0) {
            return;
        }
        level->state = 0xB;
        return;
    case 9:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x60) == 0) {
            return;
        }
        level->state++;
        return;
    case 10:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x66) == 0) {
            return;
        }
        level->state = 0x10;
        return;
    case 11:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x6C) == 0) {
            return;
        }
        if (g_wldcore_job_selection.result == 0) {
            level->state = 0xC;
            return;
        }
        level->state = 0xF;
        return;
    case 12:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x72) == 0) {
            return;
        }
        level->state++;
        return;
    case 13:
        if (level->phase == 0) {
            participant
                = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices
                      [(rand()
                           * g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count)
                          >> 15];
            if (participant == g_wldcore_job_selection.excluded_party_index) {
                g_world_text_substitution_values[1]
                    = wldcore_proposition_adjust_message_index(0xE5, participant) + 0xB800;
            } else {
                g_world_text_substitution_values[1] = participant + 0x4000;
            }
        }
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x78) == 0) {
            return;
        }
        level->state++;
        return;
    case 14:
        if (g_main_file_still_loading != 0) {
            return;
        }
        if (level->phase == 0) {
            picture = &g_wldcore_job_selection.reward_type;
            if (*picture == 1) {
                record = wldcore_window_append_render_record_and_reset_color(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                level->render_index = record;
                world_gs_gettiminfo(g_wldcore_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.position.x = image.px;
                bounds.position.y = image.py;
                bounds.dimensions.x = image.pw;
                bounds.dimensions.y = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                wldcore_window_init_vram_render_record(record, bounds.position, bounds.dimensions, clut, 4);
                g_wldcore_window_render_records[record].priority = 9;
                g_wldcore_window_render_records[record].flags |= 0x100;
                {
                    wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[record].base_x;
                    base->x = -image.pw - 4;
                    base->y = -(image.ph >> 1) + 0x1A;
                }

                record = wldcore_window_append_render_record_and_reset_color(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                bounds.position.x = 0;
                bounds.position.y = 0x70;
                bounds.dimensions.x = 0x8C;
                bounds.dimensions.y = 0x8C;
                wldcore_window_init_render_record_image(
                    record, bounds.position, bounds.dimensions, 0, 0, g_wldcore_window_image_buffer);
                g_wldcore_window_render_records[record].priority = 9;
                {
                    wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[record].base_x;
                    base->x = -0x46;
                    base->y = -0x2A;
                }
                wldcore_sound_play_effect(0x85);
            }
            if (*picture == 2) {
                record = wldcore_window_append_render_record_and_reset_color(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                level->render_index = record;
                world_gs_gettiminfo(g_wldcore_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.position.x = image.px;
                bounds.position.y = image.py;
                bounds.dimensions.x = image.pw;
                bounds.dimensions.y = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                wldcore_window_init_vram_render_record(record, bounds.position, bounds.dimensions, clut, 4);
                g_wldcore_window_render_records[record].priority = 9;
                g_wldcore_window_render_records[record].flags |= 0x100;
                {
                    wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[record].base_x;
                    base->x = -image.pw;
                    base->y = -(image.ph >> 1);
                }

                record = wldcore_window_append_render_record_and_reset_color(
                    g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                bounds.position.x = 0;
                bounds.position.y = 0x80;
                bounds.dimensions.x = 0x88;
                bounds.dimensions.y = 0x5A;
                wldcore_window_init_render_record_image(
                    record, bounds.position, bounds.dimensions, 0, 0, g_wldcore_window_image_buffer);
                g_wldcore_window_render_records[record].priority = 9;
                {
                    wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[record].base_x;
                    base->x = -0x41;
                    base->y = -0x2C;
                }
                wldcore_sound_play_effect(0x85);
            }
        }
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x7E) == 0) {
            return;
        }
        level->state = 0x10;
        return;
    case 15:
        if (wldcore_proposition_step_message((wldcore_proposition_message_state_t*)level, 0x84) == 0) {
            return;
        }
        level->state++;
        return;
    case 16:
        if (wldcore_proposition_step_message(
                (wldcore_proposition_message_state_t*)level, g_wldcore_job_selection.result * 6 + 0x8A)
            == 0) {
            return;
        }
        level->state++;
        return;
    case 17:
        if (wldcore_proposition_step_participant_message(
                (wldcore_proposition_message_state_t*)level, g_wldcore_job_selection.result * 6 + 0x24, -1)
            == 0) {
            return;
        }
        (*(s32*)level)++;
        if (g_wldcore_job_selection.reward_type == 1) {
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 2;
        }
        if (g_wldcore_job_selection.reward_type == 2) {
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 2;
        }
        world_thread_set_parameters(0xE, 0, -1, 0);
        return;
    case -1:
    case 18:
    case 99:
        if (world_thread_is_running(0xE) != 0) {
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
