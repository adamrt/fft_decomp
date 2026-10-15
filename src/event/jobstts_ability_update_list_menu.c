#include "fft/event_jobstts.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Selected job's ability list: tabbed by category, with the selected
 * ability's description text. Returns 1 while open, 0 on cancel, -1 on
 * confirm. */
s32 jobstts_ability_update_list_menu(void) {
    RECT rect;
    s16 selected_index;
    s16 scroll_index;
    s32 entry;
    s32 category;
    s32 input;

    if (g_jobstts_ability_list_initialized == 0) {
        g_jobstts_cmd_conditions[0] = menu_job_get_current_level;
        g_jobstts_cmd_conditions[1] = jobstts_job_get_current_total_jp;
        g_jobstts_cmd_conditions[2] = menu_job_get_next_level_jp_requirement;
        g_jobstts_cmd_conditions[3] = jobstts_job_get_current_jp;
        g_jobstts_cmd_conditions[4] = menu_ability_is_non_action_learned;
        g_jobstts_cmd_conditions[5] = jobstts_job_is_current_mastered;
        g_jobstts_cmd_conditions[6] = menu_ability_is_learned;
        g_jobstts_cmd_conditions[7] = menu_ability_get_mp_cost_display_value;
        g_jobstts_cmd_conditions[8] = menu_ability_get_ct_display_value;
        g_jobstts_cmd_conditions[9] = menu_ability_get_jp_cost_display_value;
        g_jobstts_cmd_conditions[10] = menu_ability_is_movement_tab;
        g_jobstts_cmd_conditions[11] = menu_ability_is_support_tab;
        g_jobstts_cmd_conditions[12] = menu_ability_is_reaction_tab;
        g_jobstts_cmd_conditions[13] = menu_ability_is_action_tab;
        g_jobstts_cmd_conditions[14] = menu_ability_is_non_action_unlearned;
        g_jobstts_cmd_conditions[15] = menu_ability_is_action;
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_ACTION);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_REACTION);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_SUPPORT);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_MOVEMENT);
        rect.x = 0x100;
        rect.y = 0xc8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_jobstts_ability_list_job_name_ids[0] = g_jobstts_job_selected_id;
        text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_jobstts_ability_list_job_name_ids, &rect, 0);
        g_menu_ability_category = jobstts_menu_update_wrapped_horizontal_selection(4, 5, 0);
        jobstts_ability_build_list(
            0, g_jobstts_job_selected_id & 0x3ff, g_menu_ability_category, g_menu_ability_entries, 0);
        jobstts_menu_init_scrollable_list_core(
            g_menu_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_jobstts_ability_list_initialized = 1;
    }

    entry = g_menu_ability_entries[g_jobstts_menu_list_selected_index];
    if (g_jobstts_menu_list_entry_count == 0) {
        g_jobstts_text_selection_id = -1;
    } else if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        g_jobstts_text_selection_id = -1;
    } else {
        g_jobstts_text_selection_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + 0x7800;
    }

    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        g_jobstts_ability_list_initialized = 0;
        g_jobstts_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (input & PSX_PAD_CIRCLE) {
        g_jobstts_ability_list_initialized = 0;
        g_jobstts_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_CLOSE;
    }

    category = jobstts_menu_update_wrapped_horizontal_selection(4, 5, g_menu_input_secondary_repeat);
    if (category != g_menu_ability_category) {
        menu_set_selection_record(g_menu_ability_category, g_jobstts_menu_list_selected_index,
            g_jobstts_menu_scroll_base_index, g_menu_ability_entries);
        g_menu_ability_category = category;
        jobstts_ability_build_list(0, g_jobstts_job_selected_id, g_menu_ability_category, g_menu_ability_entries, 0);
        menu_get_selection_record(g_menu_ability_category, &selected_index, &scroll_index, g_menu_ability_entries);
        jobstts_menu_init_scrollable_list(
            g_menu_ability_entries, selected_index, scroll_index, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        jobstts_gfx_set_transition_frame(0xa);
        g_jobstts_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    jobstts_cmd_run_stream_with_mode(
        &g_jobstts_ability_list_render_commands, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    rect.x = 0x26;
    rect.y = 0x27;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(
        &rect, 0, 0xc8, 0, 0, g_jobstts_gfx_background_texture_page, g_menu_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
