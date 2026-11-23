/* Formation learns and displays abilities in the selected Action, Reaction, Support or Movement category. */

#include "fft/game/ability.h"
#include "fft/game/battle_unit.h"
#include "fft/game/job.h"
#include "fft/menu/help.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_list.h"
#include "fft/menu/menu_screen.h"
#include "fft/menu/menu_script.h"
#include "fft/menu/menu_selection.h"
#include "fft/menu/menu_window.h"
#include "fft/party/ability_list.h"
#include "fft/party/formation.h"
#include "fft/party/formation_ability.h"
#include "fft/party/job_list.h"
#include "fft/party/unit_panel.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s16 g_formation_ability_category;

/* menu_ability_category_e */
extern s16 g_formation_ability_learn_job_name_list[];
extern menu_entry_t g_formation_ability_learn_confirm_menu[2];

/* Nonzero while the learn-confirmation menu thread (thread 6) is running. */
static u8 s_formation_ability_learn_confirm_prompt_state;
extern s8 g_formation_ability_view_step;

/* menu_job_list_step_e */
extern u8 g_formation_ability_view_confirm_prompt_state;
extern s16 g_formation_ability_view_job_names[];

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_formation_ability_list_script[493];

/* Ability-list menu-script callback 13: 1 while the list shows the Action category. The
 * row argument is ignored. */
s32 ability_list_is_action_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}

/* Ability-list menu-script callback 12: 1 while the list shows the Reaction category. */
s32 ability_list_is_reaction_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_REACTION;
}

/* Ability-list menu-script callback 11: 1 while the list shows the Support category. */
s32 ability_list_is_support_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_SUPPORT;
}

/* Ability-list menu-script callback 10: 1 while the list shows the Movement category. */
s32 ability_list_is_movement_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}

/* Report whether ability entry `index` counts as already learned: the learned
 * flag in the learn menu (FORMATION_MENU_ABILITY_LEARN), otherwise the
 * inverse of the disabled flag. */
s32 ability_list_is_learned(s32 index) {
    if (g_formation_current_menu == FORMATION_MENU_ABILITY_LEARN) {
        return g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_LEARNED;
    }
    return (g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}

/*
 * Run the learn-abilities list for the job chosen in the learn-menu job list.
 *
 * Learn-menu twin of ability_list_update. Confirming an unlearned
 * ability opens the confirmation menu thread; when it closes with choice 0
 * the ability is learned and its JP cost deducted from the unit's JP in the
 * job. Category tabs keep their own cursor (selection records 10-13). Cancel
 * returns MENU_JOB_LIST_STEP_JOBS (back to the job list), otherwise
 * MENU_JOB_LIST_STEP_ABILITIES.
 */
s32 formation_ability_update_learn_list(void) {
    s32 unused[160]; /* never referenced; reproduces the target's frame */
    RECT rect;
    s16 selected_index;
    s16 scroll_base_index;
    s32 category;
    u8 running;
    s32 input;
    s32 mode;
    s16 index;

    if (g_formation_ability_learn_ability_list_open == 0) {
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
        s_formation_ability_learn_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_selection_clear_record(10);
        menu_selection_clear_record(11);
        menu_selection_clear_record(12);
        menu_selection_clear_record(13);
        g_formation_ability_category = menu_selection_step_horizontal(4, MENU_SCREEN_CURSOR_ABILITY_TAB, 0);
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
        menu_scrollable_list_init_core(
            g_ability_list_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_formation_ability_learn_job_name_list[0] = g_formation_ability_learn_selected_job;
        text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_formation_ability_learn_job_name_list, &rect, 0);
        g_formation_ability_learn_ability_list_open = 1;
        g_formation_cursor_ot_override = 0x3c;
    }
    if (g_menu_list_entry_count == 0) {
        g_help_message_text_id = -1;
    } else {
        g_help_message_text_id
            = (g_ability_list_ability_entries[g_menu_list_selected_index] & ABILITY_LIST_ENTRY_ID_MASK)
            + HELP_TEXT_ID_ABILITY_BASE;
    }
    if (s_formation_ability_learn_confirm_prompt_state == 0) {
        if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) && g_menu_list_entry_count != 0) {
            index = g_menu_list_selected_index;
            /* DISABLED in the learn list: the unit cannot pay the JP. */
            if (g_ability_list_ability_entries[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
                message_box_show_with_sound(0xc009, SOUND_SFX_ERROR_MESSAGE);
            } else if (ability_list_is_learned(index) != 0) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                s_formation_ability_learn_confirm_prompt_state
                    = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, g_formation_ability_learn_confirm_menu);
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                g_formation_ability_learn_confirm_menu[1].selected_index = 0;
                g_sound_suppressed = 1;
            }
        } else if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
            g_formation_ability_learn_ability_list_open = 0;
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_formation_cursor_ot_override = 0;
            return MENU_JOB_LIST_STEP_JOBS;
        }
        category = menu_selection_step_horizontal_with_sound(
            4, MENU_SCREEN_CURSOR_ABILITY_TAB, g_menu_input_newly_pressed, SOUND_SFX_PAGE_SWITCH);
        if (category != g_formation_ability_category) {
            menu_selection_set_record(g_formation_ability_category + 10, g_menu_list_selected_index,
                g_menu_scroll_base_index, g_ability_list_ability_entries);
            g_formation_ability_category = category;
            ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job,
                g_formation_ability_category, g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            menu_selection_restore_record(
                g_formation_ability_category + 10, &selected_index, &scroll_base_index, g_ability_list_ability_entries);
            menu_scrollable_list_init(g_ability_list_ability_entries, selected_index, scroll_base_index,
                g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_window_set_scale_step(10);
        }
    } else {
        s_formation_ability_learn_confirm_prompt_state
            = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, g_formation_ability_learn_confirm_menu);
        if (s_formation_ability_learn_confirm_prompt_state == 0) {
            if (g_menu_screen_selection_results[5] == 0) {
                formation_ability_learn(g_unit_selected_index, g_formation_ability_learn_selected_job,
                    g_ability_list_ability_entries[g_menu_list_selected_index]);
                /* `category` doubles as the JP cost; a separate local changes the register allocation. */
                category = g_ability_data[g_ability_list_ability_entries[g_menu_list_selected_index]].jp_cost;
                g_formation_unit_pointers[g_unit_selected_index]
                    ->job_points.current.values[g_formation_ability_learn_selected_job_index] -= category;
                sound_play_sfx_on_free_channel(SOUND_SFX_ABILITY_LEARNED);
            } else if (g_menu_screen_selection_results[5] == 1) {
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            }
            g_sound_suppressed = 0;
            ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job,
                g_formation_ability_category, g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            menu_scrollable_list_init_core(g_ability_list_ability_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_window_set_scale_step(10);
            menu_screen_reset_selection_results();
        }
    }
    running = s_formation_ability_learn_confirm_prompt_state;
    input = 0;
    if (running == 0) {
        input = g_menu_input_primary_repeat;
    }
    mode = 0;
    if (g_menu_input_lock_mode != 0 || running != 0) {
        mode = 1;
    }
    menu_script_run_with_palette_mode(g_formation_ability_list_script, input, mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_menu_screen_window_tpage, g_menu_screen_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}

/* The formation screen's view-abilities menu, run each frame (sibling of update_learn_menu):
 * sets up on first entry, runs the job list (MENU_JOB_LIST_STEP_JOBS) or the chosen job's
 * ability list (MENU_JOB_LIST_STEP_ABILITIES), and returns to the unit grid on
 * MENU_JOB_LIST_STEP_CLOSE. */
void job_page_update(void) {
    if (g_formation_ability_view_initialized == 0) {
        g_formation_ability_view_initialized = 1;
        g_formation_unit_cycle_mode = 1;
        g_formation_ability_view_step = MENU_JOB_LIST_STEP_JOBS;
        g_unit_panel_status_display_thread_params.style = 1;
        g_unit_panel_numeric_panel_params.style = 1;
        g_formation_equipment_panel_thread_params.style = 1;
        menu_selection_set_value(MENU_SCREEN_CURSOR_ABILITY_TAB, 0);
        menu_selection_clear_records();
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_JOBS) {
        g_formation_ability_view_step = job_list_update();
    } else if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_ABILITIES) {
        g_formation_ability_view_step = ability_list_update();
    }
    if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_CLOSE) {
        g_unit_panel_numeric_panel_params.style = 0;
        g_formation_equipment_panel_thread_params.style = 0;
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        g_formation_unit_cycle_mode = 2;
        menu_input_clear_state();
        g_menu_screen_input_secondary_repeat_latched = 0;
        g_unit_panel_status_display_thread_params.style = 0;
        g_formation_ability_view_initialized = 0;
    }
}

/*
 * Run the view-abilities list for the job chosen in the learn-menu job list.
 *
 * The first call installs the ability menu-script callbacks, draws the job
 * name, and builds the ability list for the current category. The category
 * cursor rebuilds the list; cancel returns MENU_JOB_LIST_STEP_JOBS (back to
 * the job list) and confirm MENU_JOB_LIST_STEP_CLOSE, otherwise
 * MENU_JOB_LIST_STEP_ABILITIES.
 */
s32 ability_list_update(void) {
    s32 unused[160]; /* never referenced; reproduces the target's frame */
    RECT rect;
    s16 selected_index;
    s16 scroll_base_index;
    u16 entry;
    s32 category;

    if (g_formation_ability_view_list_initialized == 0) {
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
        g_formation_ability_view_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_selection_clear_record(10);
        menu_selection_clear_record(11);
        menu_selection_clear_record(12);
        menu_selection_clear_record(13);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_formation_ability_view_job_names[0] = g_formation_ability_learn_selected_job;
        text_render_id_rows_to_vram(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_formation_ability_view_job_names, &rect, 0);
        g_formation_ability_category = menu_selection_step_horizontal(4, MENU_SCREEN_CURSOR_ABILITY_TAB, 0);
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_scrollable_list_init_core(
            g_ability_list_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_formation_ability_view_list_initialized = 1;
    }
    if (g_menu_list_entry_count == 0
        || ((entry = g_ability_list_ability_entries[g_menu_list_selected_index]) & ABILITY_LIST_ENTRY_HIDE_DETAILS)) {
        g_help_message_text_id = -1;
    } else {
        g_help_message_text_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + HELP_TEXT_ID_ABILITY_BASE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_formation_ability_view_list_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        g_formation_ability_view_list_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    category = menu_selection_step_horizontal_with_sound(
        4, MENU_SCREEN_CURSOR_ABILITY_TAB, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
    if (category != g_formation_ability_category) {
        menu_selection_set_record(g_formation_ability_category + 10, g_menu_list_selected_index,
            g_menu_scroll_base_index, g_ability_list_ability_entries);
        g_formation_ability_category = category;
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_selection_restore_record(
            g_formation_ability_category + 10, &selected_index, &scroll_base_index, g_ability_list_ability_entries);
        menu_scrollable_list_init(g_ability_list_ability_entries, selected_index, scroll_base_index,
            g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_window_set_scale_step(10);
    }
    menu_script_run_with_palette_mode(
        g_formation_ability_list_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_menu_screen_window_tpage, g_menu_screen_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
