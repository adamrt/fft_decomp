/* The selected job's ability list, tabbed by category, with the selected ability's description. */

#include "fft/data_ability.h"
#include "fft/event_jobstts.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Selected job's ability list: tabbed by category, with the selected
 * ability's description text. Returns 1 while open, 0 on cancel, -1 on
 * confirm. */
s32 menu_ability_update_list_menu_variant(void) {
    RECT rect;
    s16 selected;
    s16 scroll;
    s32 entry;
    s32 category;
    s32 input;

    if (g_jobstts_ability_list_initialized == 0) {
        g_menu_row_callbacks[0].value = menu_job_get_current_level;
        g_menu_row_callbacks[1].value = job_get_current_total_jp;
        g_menu_row_callbacks[2].value = menu_job_get_next_level_jp_requirement;
        g_menu_row_callbacks[3].value = job_get_current_jp;
        g_menu_row_callbacks[4].value = menu_ability_is_non_action_learned;
        g_menu_row_callbacks[5].value = job_is_current_mastered;
        g_menu_row_callbacks[6].value = menu_ability_is_learned;
        g_menu_row_callbacks[7].value = menu_ability_get_mp_cost_display_value;
        g_menu_row_callbacks[8].value = menu_ability_get_ct_display_value;
        g_menu_row_callbacks[9].value = menu_ability_get_jp_cost_display_value;
        g_menu_row_callbacks[10].value = menu_ability_is_movement_tab;
        g_menu_row_callbacks[11].value = menu_ability_is_support_tab;
        g_menu_row_callbacks[12].value = menu_ability_is_reaction_tab;
        g_menu_row_callbacks[13].value = menu_ability_is_action_tab;
        g_menu_row_callbacks[14].value = menu_ability_is_non_action_unlearned;
        g_menu_row_callbacks[15].value = menu_ability_is_action;
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_ACTION);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_REACTION);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_SUPPORT);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_MOVEMENT);
        rect.x = 0x100;
        rect.y = 0xc8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_jobstts_ability_list_job_name_ids[0] = g_job_selected_id;
        text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_jobstts_ability_list_job_name_ids, &rect, 0);
        g_menu_ability_category = jobstts_menu_update_wrapped_horizontal_selection(4, 5, 0);
        jobstts_ability_build_list(0, g_job_selected_id & 0x3ff, g_menu_ability_category, g_menu_ability_entries, 0);
        menu_init_scrollable_list_core_variant(
            g_menu_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_jobstts_ability_list_initialized = 1;
    }

    entry = g_menu_ability_entries[g_menu_list_selected_index];
    if (g_menu_list_entry_count == 0) {
        g_text_selection_id = -1;
    } else if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        g_text_selection_id = -1;
    } else {
        g_text_selection_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + 0x7800;
    }

    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        g_jobstts_ability_list_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (input & PSX_PAD_CIRCLE) {
        g_jobstts_ability_list_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_CLOSE;
    }

    category = jobstts_menu_update_wrapped_horizontal_selection(4, 5, g_menu_input_secondary_repeat);
    if (category != g_menu_ability_category) {
        menu_set_selection_record(
            g_menu_ability_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_menu_ability_entries);
        g_menu_ability_category = category;
        jobstts_ability_build_list(0, g_job_selected_id, g_menu_ability_category, g_menu_ability_entries, 0);
        menu_get_selection_record_variant(g_menu_ability_category, &selected, &scroll, g_menu_ability_entries);
        menu_init_scrollable_list(
            g_menu_ability_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_set_window_scale_step(0xa);
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    menu_run_script_with_palette_mode_variant(
        &g_jobstts_ability_list_render_commands, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    rect.x = 0x26;
    rect.y = 0x27;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(
        &rect, 0, 0xc8, 0, 0, g_jobstts_gfx_background_texture_page, g_menu_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
