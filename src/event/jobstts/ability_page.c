/* Status ability page and its row callbacks. */

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
#include "fft/status_screen.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Menu callback: 1 unless the entry's disabled bit (0x4000) is set. */
s32 ability_list_is_learned(s32 index) {
    return (g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}

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

/* Ability page for the chosen job (g_job_list_selected_id): its abilities in four
 * category tabs that LEFT and RIGHT cycle through, with the highlighted ability's
 * description as the help text. The first call installs the row callbacks the render
 * script reads, clears the four categories' saved cursors and renders the job name into
 * VRAM for the page title. Returns MENU_JOB_LIST_STEP_ABILITIES while open,
 * MENU_JOB_LIST_STEP_JOBS on CROSS (back to the job page) and MENU_JOB_LIST_STEP_CLOSE on
 * CIRCLE; both exits play the cancel sound. */
s32 ability_list_update(void) {
    RECT rect;
    s16 selected;
    s16 scroll;
    s32 entry;
    s32 category;
    s32 input;

    if (g_ability_list_initialized == 0) {
        g_menu_row_callbacks[0].value = job_list_get_level;
        g_menu_row_callbacks[1].value = job_list_get_total_jp;
        g_menu_row_callbacks[2].value = job_list_get_next_level_jp;
        g_menu_row_callbacks[3].value = job_list_get_jp;
        g_menu_row_callbacks[4].value = ability_list_is_non_action_learned;
        g_menu_row_callbacks[5].value = job_list_is_mastered;
        g_menu_row_callbacks[6].value = ability_list_is_learned;
        g_menu_row_callbacks[7].value = ability_list_get_mp_cost;
        g_menu_row_callbacks[8].value = ability_list_get_ct;
        g_menu_row_callbacks[9].value = ability_list_get_jp_cost;
        g_menu_row_callbacks[10].value = ability_list_is_movement_tab;
        g_menu_row_callbacks[11].value = ability_list_is_support_tab;
        g_menu_row_callbacks[12].value = ability_list_is_reaction_tab;
        g_menu_row_callbacks[13].value = ability_list_is_action_tab;
        g_menu_row_callbacks[14].value = ability_list_is_non_action_unlearned;
        g_menu_row_callbacks[15].value = ability_list_is_action;
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_ACTION);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_REACTION);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_SUPPORT);
        menu_clear_selection_record(MENU_ABILITY_CATEGORY_MOVEMENT);
        rect.x = 0x100;
        rect.y = 0xc8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_ability_list_job_name_ids[0] = g_job_list_selected_id;
        text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_ability_list_job_name_ids, &rect, 0);
        g_ability_list_ability_category = status_screen_update_wrapped_horizontal_selection(4, 5, 0);
        ability_list_build(
            0, g_job_list_selected_id & 0x3ff, g_ability_list_ability_category, g_ability_list_ability_entries, 0);
        menu_init_scrollable_list_core(
            g_ability_list_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_ability_list_initialized = 1;
    }

    /* The help text is the ability's entry in HELP_TEXT_SECTION_ABILITIES; abilities JP cannot
     * learn show none. */
    entry = g_ability_list_ability_entries[g_menu_list_selected_index];
    if (g_menu_list_entry_count == 0) {
        g_text_selection_id = -1;
    } else if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        g_text_selection_id = -1;
    } else {
        g_text_selection_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + HELP_TEXT_ID_ABILITY_BASE;
    }

    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        g_ability_list_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (input & PSX_PAD_CIRCLE) {
        g_ability_list_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_CLOSE;
    }

    /* On a tab change, save the old tab's cursor and rebuild the list for the new one. */
    category = status_screen_update_wrapped_horizontal_selection(4, 5, g_menu_input_secondary_repeat);
    if (category != g_ability_list_ability_category) {
        menu_set_selection_record(g_ability_list_ability_category, g_menu_list_selected_index, g_menu_scroll_base_index,
            g_ability_list_ability_entries);
        g_ability_list_ability_category = category;
        ability_list_build(
            0, g_job_list_selected_id, g_ability_list_ability_category, g_ability_list_ability_entries, 0);
        menu_get_selection_record(g_ability_list_ability_category, &selected, &scroll, g_ability_list_ability_entries);
        menu_init_scrollable_list(
            g_ability_list_ability_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_set_window_scale_step(0xa);
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
    }
    menu_run_script_with_palette_mode(
        &g_ability_list_render_commands, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    /* The job name title, drawn from where the first call rendered it (0x100, 0xc8). */
    rect.x = 0x26;
    rect.y = 0x27;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(
        &rect, 0, 0xc8, 0, 0, g_status_screen_gfx_background_texture_page, g_menu_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
