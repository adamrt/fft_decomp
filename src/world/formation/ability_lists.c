/* Formation learns and displays abilities in the selected Action, Reaction, Support or Movement category. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/help.h"
#include "fft/job_list.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/message_box.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
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

static s32 view_list_update(void);

/* Ability-list menu-script callback 13: 1 while the list shows the Action category. The
 * row argument is ignored. */
static s32 action_tab_check(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}

/* Ability-list menu-script callback 12: 1 while the list shows the Reaction category. */
static s32 reaction_tab_check(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_REACTION;
}

/* Ability-list menu-script callback 11: 1 while the list shows the Support category. */
static s32 support_tab_check(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_SUPPORT;
}

/* Ability-list menu-script callback 10: 1 while the list shows the Movement category. */
static s32 movement_tab_check(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}

/* Report whether ability entry `index` counts as already learned: the learned
 * flag in the learn menu (FORMATION_MENU_ABILITY_LEARN), otherwise the
 * inverse of the disabled flag. */
static s32 learned_flag_get(s32 index) {
    if (g_formation_current_menu == FORMATION_MENU_ABILITY_LEARN) {
        return g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_LEARNED;
    }
    return (g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}

/*
 * Run the learn-abilities list for the job chosen in the learn-menu job list.
 *
 * Learn-menu twin of view_list_update. Confirming an unlearned
 * ability opens the confirmation menu thread; when it closes with choice 0
 * the ability is learned and its JP cost deducted from the unit's JP in the
 * job. Category tabs keep their own cursor (selection records 10-13). Cancel
 * returns MENU_JOB_LIST_STEP_JOBS (back to the job list), otherwise
 * MENU_JOB_LIST_STEP_ABILITIES.
 */
s32 formation_ability_learn_list_update(void) {
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
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_LEVEL].value = formation_ability_chosen_job_level_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_TOTAL_JP].value = formation_ability_chosen_job_total_jp_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NEXT_LEVEL_JP].value = formation_ability_chosen_job_next_jp_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CURRENT_JP].value = formation_ability_chosen_job_jp_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_ENABLED].value = ability_list_is_non_action_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_MASTERED].value = formation_ability_is_chosen_job_mastered;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_LEARNED].value = learned_flag_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MP_COST].value = ability_list_get_mp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CHARGE_TIME].value = ability_list_get_ct;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JP_COST].value = ability_list_get_jp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MOVEMENT_TAB].value = movement_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_SUPPORT_TAB].value = support_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_REACTION_TAB].value = reaction_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_TAB].value = action_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_DISABLED].value = ability_list_is_non_action_unlearned;
        s_formation_ability_learn_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_selection_record_clear(10);
        menu_selection_record_clear(11);
        menu_selection_record_clear(12);
        menu_selection_record_clear(13);
        g_formation_ability_category = menu_selection_horizontal_step(4, SCREEN_CURSOR_ABILITY_TAB, 0);
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
        menu_scrollable_list_core_init(
            g_ability_list_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_formation_ability_learn_job_name_list[0] = g_formation_ability_learn_selected_job;
        text_id_rows_to_vram_render(
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
            } else if (learned_flag_get(index) != 0) {
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
        category = menu_selection_horizontal_sound_step(
            4, SCREEN_CURSOR_ABILITY_TAB, g_menu_input_newly_pressed, SOUND_SFX_PAGE_SWITCH);
        if (category != g_formation_ability_category) {
            menu_selection_record_set(g_formation_ability_category + 10, g_menu_list_selected_index,
                g_menu_scroll_base_index, g_ability_list_ability_entries);
            g_formation_ability_category = category;
            ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job,
                g_formation_ability_category, g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            menu_selection_record_restore(
                g_formation_ability_category + 10, &selected_index, &scroll_base_index, g_ability_list_ability_entries);
            menu_init_scrollable_list(g_ability_list_ability_entries, selected_index, scroll_base_index,
                g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_set_window_scale_step(10);
        }
    } else {
        s_formation_ability_learn_confirm_prompt_state
            = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, g_formation_ability_learn_confirm_menu);
        if (s_formation_ability_learn_confirm_prompt_state == 0) {
            if (g_screen_selection_results[5] == 0) {
                formation_ability_learn(g_unit_selected_index, g_formation_ability_learn_selected_job,
                    g_ability_list_ability_entries[g_menu_list_selected_index]);
                /* `category` doubles as the JP cost; a separate local changes the register allocation. */
                category = g_ability_data[g_ability_list_ability_entries[g_menu_list_selected_index]].jp_cost;
                g_formation_unit_pointers[g_unit_selected_index]
                    ->job_points.current.values[g_formation_ability_learn_selected_job_index] -= category;
                sound_free_channel_sfx_play(SOUND_SFX_ABILITY_LEARNED);
            } else if (g_screen_selection_results[5] == 1) {
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            }
            g_sound_suppressed = 0;
            ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job,
                g_formation_ability_category, g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            menu_scrollable_list_core_init(g_ability_list_ability_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_set_window_scale_step(10);
            screen_selection_results_reset();
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
    menu_run_script_with_palette_mode(g_formation_ability_list_script, input, mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_screen_window_tpage, g_screen_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}

/* The formation screen's view-abilities menu, run each frame (sibling of learn_menu_update):
 * sets up on first entry, runs the job list (MENU_JOB_LIST_STEP_JOBS) or the chosen job's
 * ability list (MENU_JOB_LIST_STEP_ABILITIES), and returns to the unit grid on
 * MENU_JOB_LIST_STEP_CLOSE. */
void formation_ability_view_menu_update(void) {
    if (g_formation_ability_view_initialized == 0) {
        g_formation_ability_view_initialized = 1;
        g_formation_unit_cycle_mode = 1;
        g_formation_ability_view_step = MENU_JOB_LIST_STEP_JOBS;
        g_unit_panel_status_display_thread_params.style = 1;
        g_unit_panel_numeric_panel_params.style = 1;
        g_formation_equipment_panel_thread_params.style = 1;
        screen_cursor_position_init(SCREEN_CURSOR_ABILITY_TAB, 0);
        menu_selection_records_clear();
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_JOBS) {
        g_formation_ability_view_step = formation_ability_job_list_update();
    } else if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_ABILITIES) {
        g_formation_ability_view_step = view_list_update();
    }
    if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_CLOSE) {
        g_unit_panel_numeric_panel_params.style = 0;
        g_formation_equipment_panel_thread_params.style = 0;
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        g_formation_unit_cycle_mode = 2;
        menu_input_clear_state();
        g_screen_input_secondary_repeat_latched = 0;
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
static s32 view_list_update(void) {
    s32 unused[160]; /* never referenced; reproduces the target's frame */
    RECT rect;
    s16 selected_index;
    s16 scroll_base_index;
    u16 entry;
    s32 category;

    if (g_formation_ability_view_list_initialized == 0) {
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_LEVEL].value = formation_ability_chosen_job_level_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_TOTAL_JP].value = formation_ability_chosen_job_total_jp_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NEXT_LEVEL_JP].value = formation_ability_chosen_job_next_jp_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CURRENT_JP].value = formation_ability_chosen_job_jp_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_ENABLED].value = ability_list_is_non_action_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_MASTERED].value = formation_ability_is_chosen_job_mastered;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_LEARNED].value = learned_flag_get;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MP_COST].value = ability_list_get_mp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CHARGE_TIME].value = ability_list_get_ct;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JP_COST].value = ability_list_get_jp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MOVEMENT_TAB].value = movement_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_SUPPORT_TAB].value = support_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_REACTION_TAB].value = reaction_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_TAB].value = action_tab_check;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_DISABLED].value = ability_list_is_non_action_unlearned;
        g_formation_ability_view_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_selection_record_clear(10);
        menu_selection_record_clear(11);
        menu_selection_record_clear(12);
        menu_selection_record_clear(13);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_formation_ability_view_job_names[0] = g_formation_ability_learn_selected_job;
        text_id_rows_to_vram_render(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_formation_ability_view_job_names, &rect, 0);
        g_formation_ability_category = menu_selection_horizontal_step(4, SCREEN_CURSOR_ABILITY_TAB, 0);
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_scrollable_list_core_init(
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
    category = menu_selection_horizontal_sound_step(
        4, SCREEN_CURSOR_ABILITY_TAB, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
    if (category != g_formation_ability_category) {
        menu_selection_record_set(g_formation_ability_category + 10, g_menu_list_selected_index,
            g_menu_scroll_base_index, g_ability_list_ability_entries);
        g_formation_ability_category = category;
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_selection_record_restore(
            g_formation_ability_category + 10, &selected_index, &scroll_base_index, g_ability_list_ability_entries);
        menu_init_scrollable_list(g_ability_list_ability_entries, selected_index, scroll_base_index,
            g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_set_window_scale_step(10);
    }
    menu_run_script_with_palette_mode(
        g_formation_ability_list_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_screen_window_tpage, g_screen_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
