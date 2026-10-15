#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the selected job's ability list.
 *
 * Left/right switches between the action, reaction, support and movement
 * tabs. Cancel returns 0 with menu state 2 and confirm returns -1 with menu
 * state 1; otherwise the list and the job-name header are drawn and 1 is
 * returned. */
s32 bunit_ability_update_list_menu(void) {
    u8 unused[0x280]; /* never accessed; reserves the target's frame area */
    RECT rect;
    s16 selected;
    s16 scroll;
    u16 entry;
    s32 category;

    if (g_bunit_ability_list_ready == 0) {
        g_menu_row_callbacks[0].value = menu_job_get_current_level;
        g_menu_row_callbacks[1].value = bunit_job_get_current_total_jp;
        g_menu_row_callbacks[2].value = menu_job_get_next_level_jp_requirement;
        g_menu_row_callbacks[3].value = bunit_job_get_current_jp;
        g_menu_row_callbacks[4].value = menu_ability_is_non_action_learned;
        g_menu_row_callbacks[5].value = bunit_job_is_current_mastered;
        g_menu_row_callbacks[6].value = menu_ability_is_learned;
        g_menu_row_callbacks[7].value = menu_ability_get_mp_cost_display_value;
        g_menu_row_callbacks[8].value = menu_ability_get_ct_display_value;
        g_menu_row_callbacks[9].value = menu_ability_get_jp_cost_display_value;
        g_menu_row_callbacks[10].value = menu_ability_is_movement_tab;
        g_menu_row_callbacks[11].value = menu_ability_is_support_tab;
        g_menu_row_callbacks[12].value = menu_ability_is_reaction_tab;
        g_menu_row_callbacks[13].value = menu_ability_is_action_tab;
        g_menu_row_callbacks[14].value = menu_ability_is_non_action_unlearned;
        g_bunit_ability_list_confirm_prompt_state = 0;
        g_menu_row_callbacks[15].value = menu_ability_is_action;
        bunit_menu_clear_selection_record(10);
        bunit_menu_clear_selection_record(11);
        bunit_menu_clear_selection_record(12);
        bunit_menu_clear_selection_record(13);
        rect.x = 0x100;
        rect.y = 0xC8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_bunit_ability_list_job_name_row[0] = g_bunit_job_selected_id;
        bunit_text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_bunit_ability_list_job_name_row, &rect, 0);
        g_menu_ability_category = menu_update_wrapped_horizontal_selection(4, 0, 0);
        bunit_ability_build_list(
            g_bunit_unit_selected_index, g_bunit_job_selected_id, g_menu_ability_category, g_menu_ability_entries, 0);
        bunit_menu_init_scrollable_list_core(
            g_menu_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_bunit_ability_list_ready = 1;
    }
    if (g_bunit_menu_list_entry_count == 0
        || ((entry = g_menu_ability_entries[g_bunit_menu_list_selected_index]) & ABILITY_LIST_ENTRY_HIDE_DETAILS)) {
        g_bunit_text_selection_id = -1;
    } else {
        g_bunit_text_selection_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + 0x7800;
    }
    if (g_bunit_menu_input_active_mask & PSX_PAD_CROSS) {
        g_bunit_ability_list_ready = 0;
        g_bunit_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (g_bunit_menu_input_active_mask & PSX_PAD_CIRCLE) {
        g_bunit_ability_list_ready = 0;
        g_bunit_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    category = bunit_menu_update_horizontal_selection_and_mark_change(
        4, 0, g_bunit_menu_input_repeat_mask, MAIN_SFX_PAGE_SWITCH);
    if (category != g_menu_ability_category) {
        menu_set_selection_record(g_menu_ability_category + 10, g_bunit_menu_list_selected_index,
            g_menu_scroll_base_index, g_menu_ability_entries);
        g_menu_ability_category = category;
        bunit_ability_build_list(
            g_bunit_unit_selected_index, g_bunit_job_selected_id, g_menu_ability_category, g_menu_ability_entries, 0);
        bunit_menu_get_selection_record(g_menu_ability_category + 10, &selected, &scroll, g_menu_ability_entries);
        bunit_menu_init_scrollable_list(
            g_menu_ability_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        bunit_gfx_set_transition_frame(10);
    }
    bunit_menu_dispatch_with_override(
        &g_bunit_ability_list_menu_script, g_bunit_menu_input_active_mask, g_menu_input_lock_mode);
    rect.x = 0x26;
    rect.y = 0x2D;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xC8, 0, 0, g_menu_atlas_texture_page, g_menu_window_clut, 0xB);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
