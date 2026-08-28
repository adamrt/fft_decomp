#include "fft/battle_text.h"
#include "fft/data.h"
#include "fft/event.h"
#include "fft/jobstts.h"
#include "fft/main_sound.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Selected job's ability list: tabbed by category, with the selected
 * ability's description text. Returns 1 while open, 0 on cancel, -1 on
 * confirm. */
s32 jobstts_update_ability_list_menu(void) {
    RECT rect;
    s16 selected_index;
    s16 scroll_index;
    s32 entry;
    s32 category;
    s32 input;

    if (g_jobstts_ability_list_initialized == 0) {
        g_jobstts_cmd_conditions[0] = (jobstts_condition_t)jobstts_job_get_current_level;
        g_jobstts_cmd_conditions[1] = (jobstts_condition_t)jobstts_job_get_current_total_jp;
        g_jobstts_cmd_conditions[2] = (jobstts_condition_t)jobstts_job_get_next_level_jp_requirement;
        g_jobstts_cmd_conditions[3] = (jobstts_condition_t)jobstts_job_get_current_jp;
        g_jobstts_cmd_conditions[4] = jobstts_ability_is_non_action_learned;
        g_jobstts_cmd_conditions[5] = (jobstts_condition_t)jobstts_job_is_current_mastered;
        g_jobstts_cmd_conditions[6] = jobstts_ability_is_learned;
        g_jobstts_cmd_conditions[7] = (jobstts_condition_t)jobstts_ability_get_mp_cost_display_value;
        g_jobstts_cmd_conditions[8] = jobstts_ability_get_ct_display_value;
        g_jobstts_cmd_conditions[9] = (jobstts_condition_t)jobstts_ability_get_jp_cost_display_value;
        g_jobstts_cmd_conditions[10] = (jobstts_condition_t)jobstts_ability_is_movement_tab;
        g_jobstts_cmd_conditions[11] = (jobstts_condition_t)jobstts_ability_is_support_tab;
        g_jobstts_cmd_conditions[12] = (jobstts_condition_t)jobstts_ability_is_reaction_tab;
        g_jobstts_cmd_conditions[13] = (jobstts_condition_t)jobstts_ability_is_action_tab;
        g_jobstts_cmd_conditions[14] = jobstts_ability_is_non_action_unlearned;
        g_jobstts_cmd_conditions[15] = (jobstts_condition_t)jobstts_ability_is_action;
        jobstts_menu_clear_selection_record(0);
        jobstts_menu_clear_selection_record(1);
        jobstts_menu_clear_selection_record(2);
        jobstts_menu_clear_selection_record(3);
        rect.x = 0x100;
        rect.y = 0xc8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_jobstts_ability_list_job_name_ids[0] = g_jobstts_job_selected_id;
        jobstts_text_render_id_rows_to_vram(
            (s32)g_battle_text_section_pointers[6], g_jobstts_ability_list_job_name_ids, &rect, 0);
        g_jobstts_ability_category = jobstts_menu_update_wrapped_horizontal_selection(4, 5, 0);
        jobstts_ability_build_list(
            0, g_jobstts_job_selected_id & 0x3ff, g_jobstts_ability_category, g_jobstts_ability_entries, 0);
        jobstts_menu_init_scrollable_list_core(g_jobstts_ability_entries, 0, (s32)g_battle_text_section_pointers[14]);
        g_jobstts_ability_list_initialized = 1;
    }

    entry = g_jobstts_ability_entries[g_jobstts_menu_list_selected_index];
    if (g_jobstts_menu_list_entry_count == 0) {
        g_jobstts_text_selected_job_id = -1;
    } else if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        g_jobstts_text_selected_job_id = -1;
    } else {
        g_jobstts_text_selected_job_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + 0x7800;
    }

    input = g_jobstts_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        g_jobstts_ability_list_initialized = 0;
        g_jobstts_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (input & PSX_PAD_CIRCLE) {
        g_jobstts_ability_list_initialized = 0;
        g_jobstts_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return -1;
    }

    category = jobstts_menu_update_wrapped_horizontal_selection(4, 5, g_jobstts_input_secondary_repeat);
    if (category != g_jobstts_ability_category) {
        jobstts_menu_set_selection_record(g_jobstts_ability_category, g_jobstts_menu_list_selected_index,
            g_jobstts_menu_scroll_base_index, (const u16*)g_jobstts_ability_entries);
        g_jobstts_ability_category = category;
        jobstts_ability_build_list(
            0, g_jobstts_job_selected_id, g_jobstts_ability_category, g_jobstts_ability_entries, 0);
        jobstts_menu_get_selection_record(
            g_jobstts_ability_category, &selected_index, &scroll_index, g_jobstts_ability_entries);
        jobstts_menu_init_scrollable_list(
            g_jobstts_ability_entries, selected_index, scroll_index, g_battle_text_section_pointers[14]);
        jobstts_gfx_set_transition_frame(0xa);
        g_jobstts_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    jobstts_cmd_run_stream_with_mode(
        g_jobstts_ability_list_render_commands, (void*)g_jobstts_input_primary_repeat, g_event_mode);
    rect.x = 0x26;
    rect.y = 0x27;
    rect.w = 0x50;
    rect.h = 0x10;
    jobstts_gfx_enqueue_textured_quad(
        (urect16_t*)&rect, 0, 0xc8, 0, 0, g_jobstts_gfx_background_texture_page, g_jobstts_text_metric_0, 0xb);
    return 1;
}
