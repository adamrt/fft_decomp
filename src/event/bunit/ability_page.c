/* Unit List ability page and its row callbacks. */

#include "fft/game/battle_unit.h"
#include "fft/game/job.h"
#include "fft/menu/help.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_script.h"
#include "fft/menu/menu_selection.h"
#include "fft/menu/menu_window.h"
#include "fft/party/ability_list.h"
#include "fft/party/job_list.h"
#include "fft/party/unit_list.h"
#include "fft/sys/sound.h"
#include "fft/text/text.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_ability_list_ready;
static s8 s_ability_list_confirm_prompt_state;
extern s8 g_job_list_menu_active;
extern s8 g_job_list_menu_phase; /* menu_job_list_step_e */

static u8 s_ability_list_menu_script[493] = { 0xf, 4, 1, 0xa, 0x12, 7, 1, 0x21, 0x29, 0xc4, 0x14, 0xf, 4, 1, 0xb, 0xc,
    5, 1, 0x3c, 0, 0xa, 8, 1, 0x3c, 0xf2, 1, 0x3c, 0xf1, 4, 9, 1, 0x26, 0x1c, 0x10, 8, 0x16, 0, 4, 9, 1, 0x77, 0x1c,
    0xd, 8, 0x31, 0x10, 4, 9, 1, 0x8f, 0x1c, 0x15, 8, 0, 0x78, 4, 9, 1, 0xae, 0x1c, 0x14, 8, 0x18, 0x78, 4, 9, 1, 0xd0,
    0x1c, 0xb, 9, 0xe4, 0x20, 0xa, 8, 1, 0x3c, 0xf0, 1, 0x3c, 0xf4, 0xc, 5, 0, 0x10, 0, 4, 9, 1, 0x89, 0x2e, 6, 0xb,
    0xb4, 0x10, 0xd, 0xa, 0, 0, 1, 5, 0, 0x7b, 0x2e, 0, 0xd, 0xa, 0, 1, 4, 5, 0, 0x8f, 0x2e, 0, 0xc, 5, 1, 0x3c, 0, 2,
    7, 1, 5, 6, 2, 0, 0xa, 8, 1, 0x3c, 0xf0, 1, 0x3c, 0xf4, 4, 9, 1, 0xbb, 0x2e, 0x22, 9, 0xd8, 0x78, 0xc, 5, 0x21,
    0x3c, 0, 9, 5, 0, 0x12, 0xfd, 0xe, 4, 1, 1, 5, 9, 1, 0xb1, 0x2c, 0xa, 0xa, 0x30, 0, 0xd, 0xa, 0, 2, 4, 5, 0, 0xad,
    0x2e, 0, 0xd, 0xa, 0, 3, 4, 5, 0, 0xcb, 0x2e, 0, 0x13, 2, 0xf, 4, 1, 0xf, 0x12, 7, 1, 0x21, 0x56, 0xc4, 0x84, 0xf,
    4, 1, 0x10, 0xc, 5, 1, 0x3c, 0, 0xa, 8, 1, 0x3c, 0xf2, 1, 0x3c, 0xf1, 4, 9, 1, 0x25, 0x49, 0x1a, 9, 0, 0x20, 4, 9,
    1, 0x87, 0x49, 0xd, 9, 0xb8, 0x20, 4, 9, 1, 0xa0, 0x49, 0x19, 9, 0xb1, 0x90, 4, 9, 1, 0xcd, 0x49, 0xb, 9, 0xe4,
    0x20, 2, 7, 1, 0xd, 1, 1, 0, 9, 5, 1, 0x3c, 0xf9, 9, 5, 1, 0x3c, 0xfa, 4, 9, 1, 0x43, 0x46, 0xa, 0xe, 0x88, 0, 2, 7,
    1, 0xc, 1, 1, 0, 9, 5, 1, 0x3c, 0xf9, 9, 5, 1, 0x3c, 0xfa, 4, 9, 1, 0x52, 0x46, 0xa, 0xe, 0x94, 0, 2, 7, 1, 0xb, 1,
    1, 0, 9, 5, 1, 0x3c, 0xf9, 9, 5, 1, 0x3c, 0xfa, 4, 9, 1, 0x61, 0x46, 0xe, 0xe, 0x88, 0x11, 2, 7, 1, 0xa, 1, 1, 0, 9,
    5, 1, 0x3c, 0xf9, 9, 5, 1, 0x3c, 0xfa, 4, 9, 1, 0x74, 0x46, 0xe, 0xe, 0x98, 0x11, 0xf, 4, 1, 0x12, 0x14, 6, 0, 0x56,
    0xff, 0x84, 0x10, 0xe, 1, 0x1a, 4, 0x10, 8, 0x26, 0x5a, 0xa, 8, 0x10, 0x56, 0xc6, 1, 6, 1, 4, 2, 0, 0xa, 8, 1, 0x3c,
    0xf0, 1, 0x3c, 0xf4, 4, 9, 1, 0x8e, 0x5b, 0x27, 0xa, 0xd8, 0x84, 1, 6, 1, 0xe, 2, 0, 0xa, 8, 0, 0x10, 0xfe, 1, 0x11,
    0xfe, 4, 9, 1, 0x8e, 0x5b, 0x27, 0xa, 0xd8, 0x84, 1, 6, 1, 0xf, 2, 0, 0xd, 0xa, 0, 7, 2, 5, 0, 0x88, 0x5b, 0, 0xd,
    0xa, 0, 8, 2, 5, 0, 0xa7, 0x5b, 0, 2, 7, 1, 6, 2, 1, 0, 0xa, 8, 1, 0x3c, 0xf0, 1, 0x3c, 0xf4, 4, 9, 1, 0xbf, 0x5b,
    0x26, 8, 0xd8, 0xd0, 0xd, 0xa, 0, 9, 4, 5, 0, 0xc7, 0x5b, 0, 0x15, 2, 0x13, 2, 0x16, 0x1c };
/* The title is one mutable job ID followed by the text-row terminator. */
static s16 s_ability_list_job_name_ids[2] = { 0, -1 };

/* Byte-aligned display streams, including the terminating opcode. */

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
void job_page_update(void) {
    if (g_job_list_menu_active == 0) {
        g_job_list_menu_active = 1;
        g_job_list_menu_phase = MENU_JOB_LIST_STEP_JOBS;
        menu_selection_set_value(0, 0);
        menu_selection_clear_records();
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
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_ENABLED].value = ability_list_is_non_action_enabled;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_MASTERED].value = job_list_is_mastered;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_LEARNED].value = ability_list_is_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MP_COST].value = ability_list_get_mp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CHARGE_TIME].value = ability_list_get_ct;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JP_COST].value = ability_list_get_jp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MOVEMENT_TAB].value = ability_list_is_movement_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_SUPPORT_TAB].value = ability_list_is_support_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_REACTION_TAB].value = ability_list_is_reaction_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_TAB].value = ability_list_is_action_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_DISABLED].value = ability_list_is_non_action_disabled;
        s_ability_list_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_selection_clear_record(10);
        menu_selection_clear_record(11);
        menu_selection_clear_record(12);
        menu_selection_clear_record(13);
        setRECT(&rect, 0x100, 0xC8, 0x14, 0x10);
        s_ability_list_job_name_ids[0] = g_job_list_selected_id;
        text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], s_ability_list_job_name_ids, &rect, 0);
        g_ability_list_ability_category = menu_selection_step_horizontal(4, 0, 0);
        ability_list_build(g_unit_selected_index, g_job_list_selected_id, g_ability_list_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_scrollable_list_init_core(
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
    category = menu_selection_step_horizontal_with_sound(4, 0, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
    if (category != g_ability_list_ability_category) {
        menu_selection_set_record(g_ability_list_ability_category + 10, g_menu_list_selected_index,
            g_menu_scroll_base_index, g_ability_list_ability_entries);
        g_ability_list_ability_category = category;
        ability_list_build(g_unit_selected_index, g_job_list_selected_id, g_ability_list_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_selection_restore_record(
            g_ability_list_ability_category + 10, &selected, &scroll, g_ability_list_ability_entries);
        menu_scrollable_list_init(
            g_ability_list_ability_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_window_set_scale_step(10);
    }
    menu_script_run_with_palette_mode(s_ability_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    /* The job name title, drawn from where the first call rendered it (0x100, 0xc8). */
    setRECT(&rect, 0x26, 0x2D, 0x50, 0x10);
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xC8, 0, 0, g_menu_atlas_texture_page, g_menu_window_clut, 0xB);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
