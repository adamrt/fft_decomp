#include "fft/event_bunit.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the selected job's ability list.
 *
 * Left/right switches between the action, reaction, support and movement
 * tabs. Cancel returns 0 with menu state 2 and confirm returns -1 with menu
 * state 1; otherwise the list and the job-name header are drawn and 1 is
 * returned. */
s32 bunit_run_ability_list_menu(void) {
    u8 unused[0x280]; /* never accessed; reserves the target's frame area */
    RECT rect;
    s16 selected;
    s16 scroll;
    u16 entry;
    s32 category;

    if (g_bunit_ability_list_ready == 0) {
        g_bunit_menu_row_callbacks[0] = (bunit_menu_row_callback_t)bunit_job_get_current_level;
        g_bunit_menu_row_callbacks[1] = (bunit_menu_row_callback_t)bunit_job_get_current_total_jp;
        g_bunit_menu_row_callbacks[2] = (bunit_menu_row_callback_t)bunit_job_get_next_level_jp_requirement;
        g_bunit_menu_row_callbacks[3] = (bunit_menu_row_callback_t)bunit_job_get_current_jp;
        g_bunit_menu_row_callbacks[4] = bunit_ability_is_non_action_learned;
        g_bunit_menu_row_callbacks[5] = (bunit_menu_row_callback_t)bunit_job_is_current_mastered;
        g_bunit_menu_row_callbacks[6] = bunit_ability_is_learned;
        g_bunit_menu_row_callbacks[7] = (bunit_menu_row_callback_t)bunit_ability_get_mp_cost_display_value;
        g_bunit_menu_row_callbacks[8] = bunit_ability_get_ct_display_value;
        g_bunit_menu_row_callbacks[9] = (bunit_menu_row_callback_t)bunit_ability_get_jp_cost_display_value;
        g_bunit_menu_row_callbacks[10] = (bunit_menu_row_callback_t)bunit_ability_is_movement_tab;
        g_bunit_menu_row_callbacks[11] = (bunit_menu_row_callback_t)bunit_ability_is_support_tab;
        g_bunit_menu_row_callbacks[12] = (bunit_menu_row_callback_t)bunit_ability_is_reaction_tab;
        g_bunit_menu_row_callbacks[13] = (bunit_menu_row_callback_t)bunit_ability_is_action_tab;
        g_bunit_menu_row_callbacks[14] = bunit_ability_is_non_action_unlearned;
        g_bunit_ability_list_confirm_prompt_state = 0;
        g_bunit_menu_row_callbacks[15] = (bunit_menu_row_callback_t)bunit_ability_is_action;
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
            (s32)g_battle_text_section_pointers[6], (u16*)g_bunit_ability_list_job_name_row, &rect, 0);
        g_bunit_ability_category = bunit_menu_update_wrapped_horizontal_selection(4, 0, 0);
        bunit_create_ability_list(
            g_bunit_unit_selected_index, g_bunit_job_selected_id, g_bunit_ability_category, g_bunit_ability_entries, 0);
        bunit_menu_init_scrollable_list_core(g_bunit_ability_entries, 0, (s32)g_battle_text_section_pointers[14]);
        g_bunit_ability_list_ready = 1;
    }
    if (*(s16*)&g_bunit_menu_list_entry_count == 0
        || ((entry = g_bunit_ability_entries[g_bunit_menu_list_selected_index]) & ABILITY_LIST_ENTRY_HIDE_DETAILS)) {
        g_bunit_text_selection_id = -1;
    } else {
        g_bunit_text_selection_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + 0x7800;
    }
    if (g_bunit_menu_input_active_mask & PSX_PAD_CROSS) {
        g_bunit_ability_list_ready = 0;
        g_bunit_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_bunit_menu_input_active_mask & PSX_PAD_CIRCLE) {
        g_bunit_ability_list_ready = 0;
        g_bunit_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        return -1;
    }
    /* The target uses v0 from this void callee. */
    category = ((s32 (*)(s32, s32, s32, s32))bunit_menu_update_horizontal_selection_and_mark_change)(
        4, 0, g_bunit_menu_input_repeat_mask, 6);
    if (category != g_bunit_ability_category) {
        bunit_menu_set_selection_record(g_bunit_ability_category + 10, g_bunit_menu_list_selected_index,
            g_bunit_menu_scroll_base_index, (u16*)g_bunit_ability_entries);
        g_bunit_ability_category = category;
        bunit_create_ability_list(
            g_bunit_unit_selected_index, g_bunit_job_selected_id, (s16)category, g_bunit_ability_entries, 0);
        bunit_menu_get_selection_record(g_bunit_ability_category + 10, &selected, &scroll, g_bunit_ability_entries);
        bunit_menu_init_scrollable_list(
            (s32)g_bunit_ability_entries, selected, scroll, (s32)g_battle_text_section_pointers[14]);
        bunit_gfx_set_transition_frame(10);
    }
    bunit_menu_dispatch_with_override(
        (s32)&g_bunit_ability_list_menu_script, g_bunit_menu_input_active_mask, g_event_mode);
    rect.x = 0x26;
    rect.y = 0x2D;
    rect.w = 0x50;
    rect.h = 0x10;
    bunit_gfx_enqueue_textured_quad(&rect, 0, 0xC8, 0, 0, g_bunit_text_digit_texture_page, g_bunit_text_metric_0, 0xB);
    return 1;
}
