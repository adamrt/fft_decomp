/* Unit List ability page and its row callbacks. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/help.h"
#include "fft/job_list.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_list.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_ability_list_ready;
extern s8 g_ability_list_confirm_prompt_state;
extern s8 g_job_list_menu_active;
extern s8 g_job_list_menu_phase; /* menu_job_list_step_e */

/* The title is one mutable job ID followed by the text-row terminator. */
static s16 g_ability_list_job_name_ids[2] = { 0, -1 };

/* Byte-aligned display streams, including the terminating opcode. */
extern u8 g_ability_list_menu_script[493];

/* Row callback: return whether the ability list shows the action category; the row is ignored. */
s32 ability_list_is_action_tab(s32 unused_row) {
    return g_ability_list_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}

/* Row callback: return whether the ability list shows the reaction category; the row is ignored. */
s32 ability_list_is_reaction_tab(s32 unused_row) {
    return g_ability_list_ability_category == MENU_ABILITY_CATEGORY_REACTION;
}

/* Row callback: return whether the ability list shows the support category; the row is ignored. */
s32 ability_list_is_support_tab(s32 unused_row) {
    return g_ability_list_ability_category == MENU_ABILITY_CATEGORY_SUPPORT;
}

/* Row callback: return whether the ability list shows the movement category; the row is ignored. */
s32 ability_list_is_movement_tab(s32 unused_row) {
    return g_ability_list_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}

/* Menu callback: 1 unless the entry's disabled bit (0x4000) is set. */
s32 ability_list_is_learned(s32 index) {
    return (g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}

/* Per-frame handler of the unit list's job page: runs the job list and,
 * once a job is chosen, its ability list, and returns to the unit list when
 * either step reports MENU_JOB_LIST_STEP_CLOSE. */
void unit_list_update_job_page(void) {
    if (g_job_list_menu_active == 0) {
        g_job_list_menu_active = 1;
        g_job_list_menu_phase = MENU_JOB_LIST_STEP_JOBS;
        menu_selection_value_set(0, 0);
        menu_selection_records_clear();
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (g_job_list_menu_phase == MENU_JOB_LIST_STEP_JOBS) {
        g_job_list_menu_phase = job_list_update();
    } else if (g_job_list_menu_phase == MENU_JOB_LIST_STEP_ABILITIES) {
        g_job_list_menu_phase = ability_list_update();
    }
    if (g_job_list_menu_phase == MENU_JOB_LIST_STEP_CLOSE) {
        g_unit_list_current_menu = UNIT_LIST_MENU_UNIT_LIST;
        menu_input_clear_state();
        g_job_list_menu_active = 0;
    }
}

/* Runs the chosen job's ability list (g_job_list_selected_id) for the selected unit.
 *
 * Left/right switches between the action, reaction, support and movement
 * tabs, each keeping its cursor in selection records 10 to 13. The first call
 * installs the row callbacks the list script reads and renders the job name
 * into VRAM for the title. CROSS returns MENU_JOB_LIST_STEP_JOBS (back to the
 * job list) and CIRCLE MENU_JOB_LIST_STEP_CLOSE; otherwise the list and the
 * job-name title are drawn and MENU_JOB_LIST_STEP_ABILITIES is returned. */
s32 ability_list_update(void) {
    u8 unused[0x280]; /* never accessed; reserves the target's frame area */
    RECT rect;
    s16 selected;
    s16 scroll;
    u16 entry;
    s32 category;

    if (g_ability_list_ready == 0) {
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_LEVEL].value = job_list_get_level;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_TOTAL_JP].value = job_list_get_total_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NEXT_LEVEL_JP].value = job_list_get_next_level_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CURRENT_JP].value = job_list_get_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_ENABLED].value = ability_list_is_non_action_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_MASTERED].value = job_list_is_mastered;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_LEARNED].value = ability_list_is_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MP_COST].value = ability_list_get_mp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CHARGE_TIME].value = ability_list_get_ct;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JP_COST].value = ability_list_get_jp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MOVEMENT_TAB].value = ability_list_is_movement_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_SUPPORT_TAB].value = ability_list_is_support_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_REACTION_TAB].value = ability_list_is_reaction_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_TAB].value = ability_list_is_action_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_DISABLED].value = ability_list_is_non_action_unlearned;
        g_ability_list_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_selection_record_clear(10);
        menu_selection_record_clear(11);
        menu_selection_record_clear(12);
        menu_selection_record_clear(13);
        rect.x = 0x100;
        rect.y = 0xC8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_ability_list_job_name_ids[0] = g_job_list_selected_id;
        text_id_rows_to_vram_render(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_ability_list_job_name_ids, &rect, 0);
        g_ability_list_ability_category = menu_selection_horizontal_step(4, 0, 0);
        ability_list_build(g_unit_selected_index, g_job_list_selected_id, g_ability_list_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_scrollable_list_core_init(
            g_ability_list_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_ability_list_ready = 1;
    }
    /* The help text is the ability's entry in HELP_TEXT_SECTION_ABILITIES; abilities JP cannot
     * learn show none. */
    if (g_menu_list_entry_count == 0
        || ((entry = g_ability_list_ability_entries[g_menu_list_selected_index]) & ABILITY_LIST_ENTRY_HIDE_DETAILS)) {
        g_text_selection_id = -1;
    } else {
        g_text_selection_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + HELP_TEXT_ID_ABILITY_BASE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_ability_list_ready = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        g_ability_list_ready = 0;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    category = menu_selection_horizontal_sound_step(4, 0, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
    if (category != g_ability_list_ability_category) {
        menu_selection_record_set(g_ability_list_ability_category + 10, g_menu_list_selected_index,
            g_menu_scroll_base_index, g_ability_list_ability_entries);
        g_ability_list_ability_category = category;
        ability_list_build(g_unit_selected_index, g_job_list_selected_id, g_ability_list_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_selection_record_restore(
            g_ability_list_ability_category + 10, &selected, &scroll, g_ability_list_ability_entries);
        menu_init_scrollable_list(
            g_ability_list_ability_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_set_window_scale_step(10);
    }
    menu_run_script_with_palette_mode(g_ability_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    /* The job name title, drawn from where the first call rendered it (0x100, 0xc8). */
    rect.x = 0x26;
    rect.y = 0x2D;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xC8, 0, 0, g_menu_atlas_texture_page, g_menu_window_clut, 0xB);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
