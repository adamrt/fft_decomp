/* The selected job's ability list, with tabs for action, reaction, support and movement abilities. */

#include "fft/data_ability.h"
#include "fft/event_bunit.h"
#include "fft/job.h"
#include "fft/main_sound.h"
#include "fft/menu_gfx.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the chosen job's ability list (g_job_selected_id) for the selected unit.
 *
 * Left/right switches between the action, reaction, support and movement
 * tabs, each keeping its cursor in selection records 10 to 13. The first call
 * installs the row callbacks the list script reads and renders the job name
 * into VRAM for the title. CROSS returns MENU_JOB_LIST_STEP_JOBS (back to the
 * job list) and CIRCLE MENU_JOB_LIST_STEP_CLOSE; otherwise the list and the
 * job-name title are drawn and MENU_JOB_LIST_STEP_ABILITIES is returned. */
s32 bunit_menu_ability_update_list_menu(void) {
    u8 unused[0x280]; /* never accessed; reserves the target's frame area */
    RECT rect;
    s16 selected;
    s16 scroll;
    u16 entry;
    s32 category;

    if (g_bunit_ability_list_ready == 0) {
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
        g_bunit_ability_list_confirm_prompt_state = 0;
        g_menu_row_callbacks[15].value = menu_ability_is_action;
        menu_clear_selection_record(10);
        menu_clear_selection_record(11);
        menu_clear_selection_record(12);
        menu_clear_selection_record(13);
        rect.x = 0x100;
        rect.y = 0xC8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_bunit_ability_list_job_name_row[0] = g_job_selected_id;
        text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_bunit_ability_list_job_name_row, &rect, 0);
        g_menu_ability_category = menu_update_wrapped_horizontal_selection(4, 0, 0);
        ability_build_list(g_unit_selected_index, g_job_selected_id, g_menu_ability_category, g_menu_ability_entries,
            ABILITY_LIST_MODE_VIEW_ALL);
        menu_init_scrollable_list_core(g_menu_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_bunit_ability_list_ready = 1;
    }
    /* The help text is the ability's entry in HELP_TEXT_SECTION_ABILITIES; abilities JP cannot
     * learn show none. */
    if (g_menu_list_entry_count == 0
        || ((entry = g_menu_ability_entries[g_menu_list_selected_index]) & ABILITY_LIST_ENTRY_HIDE_DETAILS)) {
        g_text_selection_id = -1;
    } else {
        g_text_selection_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + HELP_TEXT_ID_ABILITY_BASE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_bunit_ability_list_ready = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        g_bunit_ability_list_ready = 0;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    category
        = menu_update_horizontal_selection_and_mark_change(4, 0, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
    if (category != g_menu_ability_category) {
        menu_set_selection_record(
            g_menu_ability_category + 10, g_menu_list_selected_index, g_menu_scroll_base_index, g_menu_ability_entries);
        g_menu_ability_category = category;
        ability_build_list(g_unit_selected_index, g_job_selected_id, g_menu_ability_category, g_menu_ability_entries,
            ABILITY_LIST_MODE_VIEW_ALL);
        menu_get_selection_record(g_menu_ability_category + 10, &selected, &scroll, g_menu_ability_entries);
        menu_init_scrollable_list(
            g_menu_ability_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_set_window_scale_step(10);
    }
    menu_run_script_with_palette_mode(
        &g_bunit_ability_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    /* The job name title, drawn from where the first call rendered it (0x100, 0xc8). */
    rect.x = 0x26;
    rect.y = 0x2D;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xC8, 0, 0, g_menu_atlas_texture_page, g_menu_window_clut, 0xB);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
