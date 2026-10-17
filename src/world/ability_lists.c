/* Ability category checks, the learn and view ability lists, unit ordering, text entry waits, the dismiss unit step and
 * gil adjustment. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/job.h"
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
#include "fft/script_variable.h"
#include "fft/shop.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_formation_ability_list_script[493];

/* Special identities whose Brave Story profile is marked when dismissed. */
typedef struct formation_dismiss_entry {
    s16 character_identity;
    s16 variable_id; /* EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST range */
} formation_dismiss_entry_t;

static formation_dismiss_entry_t g_formation_dismiss_entries[] = { { CHARACTER_IDENTITY_AGRIAS_JOIN, 0x3ce },
    { CHARACTER_IDENTITY_AGRIAS_GUEST, 0x3ce }, { CHARACTER_IDENTITY_ORLANDU, 0x3cf },
    { CHARACTER_IDENTITY_BEOWULF, 0x3d6 }, { CHARACTER_IDENTITY_REIS_HUMAN, 0x3d8 },
    { CHARACTER_IDENTITY_REIS_DRAGON, 0x3d8 }, { CHARACTER_IDENTITY_RAFA_JOIN, 0x3dc },
    { CHARACTER_IDENTITY_MALAK, 0x3dd }, { CHARACTER_IDENTITY_MUSTADIO_JOIN, 0x3e2 },
    { CHARACTER_IDENTITY_MUSTADIO_GUEST, 0x3e2 }, { CHARACTER_IDENTITY_BALMAFULA, 0x3d9 },
    { CHARACTER_IDENTITY_MELIADOUL_JOIN, 0x3ed }, { CHARACTER_IDENTITY_CLOUD, 0x3ee }, { -1, 0 } };

static s32 formation_ability_view_list(void);

/* Ability-list menu-script callback 13: 1 while the list shows the Action category. The
 * row argument is ignored. */
static s32 formation_ability_is_action_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}

/* Ability-list menu-script callback 12: 1 while the list shows the Reaction category. */
static s32 formation_ability_is_reaction_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_REACTION;
}

/* Ability-list menu-script callback 11: 1 while the list shows the Support category. */
static s32 formation_ability_is_support_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_SUPPORT;
}

/* Ability-list menu-script callback 10: 1 while the list shows the Movement category. */
static s32 formation_ability_is_movement_tab(s32 unused_row) {
    return g_formation_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}

/* Report whether ability entry `index` counts as already learned: the learned
 * flag in the learn menu (FORMATION_MENU_ABILITY_LEARN), otherwise the
 * inverse of the disabled flag. */
static s32 formation_ability_is_learned(s32 index) {
    if (g_formation_current_menu == FORMATION_MENU_ABILITY_LEARN) {
        return g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_LEARNED;
    }
    return (g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}

/*
 * Run the learn-abilities list for the job chosen in the learn-menu job list.
 *
 * Learn-menu twin of formation_ability_view_list. Confirming an unlearned
 * ability opens the confirmation menu thread; when it closes with choice 0
 * the ability is learned and its JP cost deducted from the unit's JP in the
 * job. Category tabs keep their own cursor (selection records 10-13). Cancel
 * returns MENU_JOB_LIST_STEP_JOBS (back to the job list), otherwise
 * MENU_JOB_LIST_STEP_ABILITIES.
 */
s32 formation_ability_learn_list(void) {
    s32 unused[160]; /* never referenced; reproduces the target's frame */
    RECT rect;
    s16 x;
    s16 y;
    s32 category;
    u8 running;
    s32 input;
    s32 mode;
    s16 index;

    if (g_formation_ability_learn_ability_list_open == 0) {
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_LEVEL].value = formation_ability_get_chosen_job_level;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_TOTAL_JP].value = formation_ability_get_chosen_job_total_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NEXT_LEVEL_JP].value = formation_ability_get_chosen_job_next_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CURRENT_JP].value = formation_ability_get_chosen_job_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_ENABLED].value = ability_list_is_non_action_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_MASTERED].value = formation_ability_is_chosen_job_mastered;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_LEARNED].value = formation_ability_is_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MP_COST].value = ability_list_get_mp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CHARGE_TIME].value = ability_list_get_ct;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JP_COST].value = ability_list_get_jp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MOVEMENT_TAB].value = formation_ability_is_movement_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_SUPPORT_TAB].value = formation_ability_is_support_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_REACTION_TAB].value = formation_ability_is_reaction_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_TAB].value = formation_ability_is_action_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_DISABLED].value = ability_list_is_non_action_unlearned;
        g_formation_ability_learn_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_clear_selection_record(10);
        menu_clear_selection_record(11);
        menu_clear_selection_record(12);
        menu_clear_selection_record(13);
        g_formation_ability_category = menu_update_wrapped_horizontal_selection(4, SCREEN_CURSOR_ABILITY_TAB, 0);
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
        menu_init_scrollable_list_core(
            g_ability_list_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_formation_ability_learn_job_name_list[0] = g_formation_ability_learn_selected_job;
        screen_text_render_rows(
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
    if (g_formation_ability_learn_confirm_prompt_state == 0) {
        if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) && g_menu_list_entry_count != 0) {
            index = g_menu_list_selected_index;
            /* DISABLED in the learn list: the unit cannot pay the JP. */
            if (g_ability_list_ability_entries[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
                message_box_show_with_sound(0xc009, SOUND_SFX_ERROR_MESSAGE);
            } else if (formation_ability_is_learned(index) != 0) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                g_formation_ability_learn_confirm_prompt_state
                    = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_formation_ability_learn_confirm_menu);
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
        category = menu_update_horizontal_selection_and_mark_change(
            4, SCREEN_CURSOR_ABILITY_TAB, g_menu_input_newly_pressed, SOUND_SFX_PAGE_SWITCH);
        if (category != g_formation_ability_category) {
            menu_set_selection_record(g_formation_ability_category + 10, g_menu_list_selected_index,
                g_menu_scroll_base_index, g_ability_list_ability_entries);
            g_formation_ability_category = category;
            ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job,
                g_formation_ability_category, g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            menu_get_selection_record(g_formation_ability_category + 10, &x, &y, g_ability_list_ability_entries);
            menu_init_scrollable_list(
                g_ability_list_ability_entries, x, y, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_set_window_scale_step(10);
        }
    } else {
        g_formation_ability_learn_confirm_prompt_state
            = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_formation_ability_learn_confirm_menu);
        if (g_formation_ability_learn_confirm_prompt_state == 0) {
            if (g_screen_selection_results[5] == 0) {
                formation_ability_learn(g_unit_selected_index, g_formation_ability_learn_selected_job,
                    g_ability_list_ability_entries[g_menu_list_selected_index]);
                /* `category` doubles as the JP cost; a separate local changes the register allocation. */
                category = g_ability_data[g_ability_list_ability_entries[g_menu_list_selected_index]].jp_cost;
                g_formation_unit_pointers[g_unit_selected_index]
                    ->job_points.current.values[g_formation_ability_learn_selected_job_index] -= category;
                sound_play_sfx_on_free_channel(SOUND_SFX_ABILITY_LEARNED);
            } else if (g_screen_selection_results[5] == 1) {
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            }
            g_sound_suppressed = 0;
            ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job,
                g_formation_ability_category, g_ability_list_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            menu_init_scrollable_list_core(g_ability_list_ability_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_set_window_scale_step(10);
            screen_reset_selection_results();
        }
    }
    running = g_formation_ability_learn_confirm_prompt_state;
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

/* The formation screen's view-abilities menu, run each frame (sibling of formation_ability_learn_menu):
 * sets up on first entry, runs the job list (MENU_JOB_LIST_STEP_JOBS) or the chosen job's
 * ability list (MENU_JOB_LIST_STEP_ABILITIES), and returns to the unit grid on
 * MENU_JOB_LIST_STEP_CLOSE. */
void formation_ability_view_menu(void) {
    if (g_formation_ability_view_initialized == 0) {
        g_formation_ability_view_initialized = 1;
        g_formation_unit_cycle_mode = 1;
        g_formation_ability_view_step = MENU_JOB_LIST_STEP_JOBS;
        g_unit_panel_status_display_thread_params.style = 1;
        g_unit_panel_numeric_panel_params.style = 1;
        g_formation_equipment_panel_thread_params.style = 1;
        screen_init_cursor_position(SCREEN_CURSOR_ABILITY_TAB, 0);
        menu_clear_selection_records();
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_JOBS) {
        g_formation_ability_view_step = formation_ability_learn_job_list();
    } else if (g_formation_ability_view_step == MENU_JOB_LIST_STEP_ABILITIES) {
        g_formation_ability_view_step = formation_ability_view_list();
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
static s32 formation_ability_view_list(void) {
    s32 unused[160]; /* never referenced; reproduces the target's frame */
    RECT rect;
    s16 x;
    s16 y;
    u16 entry;
    s32 category;

    if (g_formation_ability_view_list_initialized == 0) {
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_LEVEL].value = formation_ability_get_chosen_job_level;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_TOTAL_JP].value = formation_ability_get_chosen_job_total_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NEXT_LEVEL_JP].value = formation_ability_get_chosen_job_next_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CURRENT_JP].value = formation_ability_get_chosen_job_jp;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_ENABLED].value = ability_list_is_non_action_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JOB_MASTERED].value = formation_ability_is_chosen_job_mastered;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_LEARNED].value = formation_ability_is_learned;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MP_COST].value = ability_list_get_mp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_CHARGE_TIME].value = ability_list_get_ct;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_JP_COST].value = ability_list_get_jp_cost;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_MOVEMENT_TAB].value = formation_ability_is_movement_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_SUPPORT_TAB].value = formation_ability_is_support_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_REACTION_TAB].value = formation_ability_is_reaction_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_TAB].value = formation_ability_is_action_tab;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_NON_ACTION_DISABLED].value = ability_list_is_non_action_unlearned;
        g_formation_ability_view_confirm_prompt_state = 0;
        g_menu_row_callbacks[ABILITY_PAGE_ROW_ACTION_ABILITY].value = ability_list_is_action;
        menu_clear_selection_record(10);
        menu_clear_selection_record(11);
        menu_clear_selection_record(12);
        menu_clear_selection_record(13);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_formation_ability_view_job_names[0] = g_formation_ability_learn_selected_job;
        screen_text_render_rows(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_formation_ability_view_job_names, &rect, 0);
        g_formation_ability_category = menu_update_wrapped_horizontal_selection(4, SCREEN_CURSOR_ABILITY_TAB, 0);
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_init_scrollable_list_core(
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
    category = menu_update_horizontal_selection_and_mark_change(
        4, SCREEN_CURSOR_ABILITY_TAB, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
    if (category != g_formation_ability_category) {
        menu_set_selection_record(g_formation_ability_category + 10, g_menu_list_selected_index,
            g_menu_scroll_base_index, g_ability_list_ability_entries);
        g_formation_ability_category = category;
        ability_list_build(g_unit_selected_index, g_formation_ability_learn_selected_job, g_formation_ability_category,
            g_ability_list_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_get_selection_record(g_formation_ability_category + 10, &x, &y, g_ability_list_ability_entries);
        menu_init_scrollable_list(
            g_ability_list_ability_entries, x, y, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
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

/*
 * Per-frame handler of the formation screen's "order" (sort) menu.
 *
 * The menu lists the seven sort keys in their current priority order.
 * Choosing a row moves that key to the front, shifting the rows above it
 * down one, stores the order back and re-sorts the roster. While the menu is
 * open the unit banner, unit browsing and the two detail panels are off; the
 * handler returns to the unit grid when the menu thread ends.
 */
void formation_order_menu(void) {
    s16 option_ids[12]; /* seven entries plus terminator; sized for the retail frame */
    s32 index;
    s32 selected_entry;

    if (g_formation_order_open == 0) {
        item_load_type_order(0, g_formation_order_entries);
        for (index = 0; index < FORMATION_SORT_KEY_COUNT; index++) {
            option_ids[index] = g_formation_order_entries[index] + 14;
        }
        option_ids[FORMATION_SORT_KEY_COUNT] = -1;
        text_concatenate_entries(g_formation_menu_text_table, g_formation_menu_text_buffer, option_ids, 1);
        g_formation_unit_banner_enabled = 0;
        g_formation_order_saved_browse_enabled = g_formation_unit_browse_enabled;
        g_formation_unit_browse_enabled = 0;
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
        g_formation_order_menu.selected_index = 0;
        g_formation_order_open = 1;
    }

    if (menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, &g_formation_order_menu) == 0) {
        g_formation_order_open = 0;
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        g_formation_unit_browse_enabled = g_formation_order_saved_browse_enabled;
    }

    g_help_message_text_id = g_formation_order_entries[g_formation_order_menu.selected_index] + 0x103f;
    if (g_screen_selection_results[9] != -1) {
        /* Move the chosen entry to the front and redraw the options. */
        index = g_screen_selection_results[9];
        selected_entry = g_formation_order_entries[index];
        index -= 1;
        for (; index >= 0; index--) {
            g_formation_order_entries[index + 1] = g_formation_order_entries[index];
        }
        g_formation_order_entries[0] = selected_entry;
        screen_reset_selection_results();
        for (index = 0; index < FORMATION_SORT_KEY_COUNT; index++) {
            option_ids[index] = g_formation_order_entries[index] + 14;
        }
        option_ids[FORMATION_SORT_KEY_COUNT] = -1;
        text_concatenate_entries(g_formation_menu_text_table, g_formation_menu_text_buffer, option_ids, 1);
        g_screen_text_redraw_request = 1;
        item_store_type_order(0, g_formation_order_entries);
        formation_sort_roster(0);
    }
}

/* Message entry thread whose speaker is the selected formation unit's
 * roster slot. */
void formation_unit_message_thread(void) {
    /* The retail code reads roster_slot with lhu; keep the byte-exact width. */
    g_formation_selected_unit_text_binding.speaker_id
        = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;
    menu_text_entry_wait_thread();
}

/*
 * Per-frame step of the "dismiss unit" menu
 * (FORMATION_MENU_DISMISS_UNIT), called by formation_grid_menu.
 * Three phases: first entry, which picks the prompt text and menu record;
 * the per-frame poll of the prompt or menu thread; and the dismissal timer.
 *
 * On entry a unit that is away on a proposition only gets message 0xC012.
 * Otherwise g_formation_dismiss_step takes
 * formation_can_dismiss's answer: 0 shows the unit's refusal line
 * and returns to the grid, nonzero runs the yes/no confirmation. The line
 * comes from the dismiss-reply text (TEXT_ID_FORMATION_TEXT_BASE + n): identities from Girl
 * upward draw one of eight lines by gender or monster kind. Earlier identities, including Boy,
 * use their identity + 0x17 for the farewell and + 0x61 for refusal. Dragon Reis uses the
 * following identity's text slots.
 *
 * Confirming plays the dismiss sound and starts a 16-frame timer (the unit
 * index is carried in its high byte); when it runs out every
 * g_formation_dismiss_entries row matching the unit's sprite set has
 * its script variable set to 0xC and the unit is removed.
 */
void formation_dismiss_menu(void) {
    formation_unit_t* unit;
    menu_entry_t* menu;
    s32 i;
    s32 value;
    s32 sprite;
    s32 flags;
    s32 step;
    /* The sum `value + TEXT_ID_FORMATION_TEXT_BASE` is its own variable: the target's a0 carries the
     * rand()/sprite accumulation, dies at `addu a1,a0,v0`, and a0 is then reused
     * for the 0x19/0x1a prompt kind. Non-overlapping roles in one register are
     * one variable, so the kind is carried in `value` and only the sum is split
     * out here. Pinning a separate `kind` to $4 reaches the same stores but
     * displaces `value` out of a0 across the whole rand() block; the split
     * alone, without the reuse, has no effect, because `combine_regs` ties the
     * sum to its dying summand. */
    s32 text_id;
    s32 selection;
    s8 running;

    /* The timer's low byte counts the 16 frames; its high byte only carries the unit index. */
    if (g_formation_dismiss_anim_timer != 0) {
        g_formation_dismiss_anim_timer++;
        if ((g_formation_dismiss_anim_timer & 0xFF) < 0x11) {
            return;
        }
        for (i = 0; g_formation_dismiss_entries[i].character_identity != -1; i++) {
            if (g_formation_unit_pointers[g_unit_selected_index]->sprite_set
                == g_formation_dismiss_entries[i].character_identity) {
                script_set_variable(g_formation_dismiss_entries[i].variable_id, 0xC);
            }
        }
        formation_dismiss(g_unit_selected_index);
        g_sound_suppressed = 0;
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        return;
    }

    if (g_formation_dismiss_prompt_running == 0) {
        sprite = g_unit_selected_index;
        if (g_formation_unit_pointers[sprite]->proposition_status != 0) {
            message_box_show_with_sound(0xC012, SOUND_SFX_ERROR_MESSAGE);
            g_formation_dismiss_step = 2;
            g_formation_dismiss_prompt_running = 1;
        } else {
            g_formation_dismiss_step = formation_can_dismiss(sprite);
            screen_text_bind_section(SCREEN_TEXT_DATA_DISMISS_UNIT_REPLIES);
            if (g_formation_unit_browse_enabled != 0) {
                g_formation_unit_cycle_mode = 1;
            }
            g_text_substitution_values[0] = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;

            /* The random-reply range begins at Girl; Boy still takes the indexed reply path. */
            unit = g_formation_unit_pointers[g_unit_selected_index];
            sprite = unit->sprite_set;
            if (sprite >= CHARACTER_IDENTITY_GIRL) {
                flags = unit->gender_flags;
                if (flags & UNIT_FLAG_MALE) {
                    value = rand() % 8;
                } else if (flags & UNIT_FLAG_FEMALE) {
                    value = rand() % 8 + 8;
                } else {
                    value = rand() % 8 + 0x10;
                }
            } else {
                if (sprite == CHARACTER_IDENTITY_REIS_DRAGON) {
                    sprite = CHARACTER_IDENTITY_REIS_DRAGON + 1;
                }
                if (g_formation_dismiss_step != 0) {
                    value = sprite + 0x17;
                } else {
                    value = sprite + 0x61;
                }
            }
            text_id = value + TEXT_ID_FORMATION_TEXT_BASE;

            /* A refusal is a plain prompt; a dismissable unit gets the yes/no menu with its farewell. */
            if (g_formation_dismiss_step == 0) {
                g_sound_suppressed = 1;
                g_formation_dismiss_menu_data = &g_formation_dismiss_prompt;
                if (g_formation_unit_banner_at_bottom != 0) {
                    value = 0x19;
                } else {
                    value = 0x1A;
                }
                g_formation_dismiss_prompt.header_id = value;
                g_formation_dismiss_prompt.text_id = text_id;
            } else {
                menu = g_formation_dismiss_confirm_menu;
                if (g_formation_unit_banner_at_bottom != 0) {
                    menu = g_formation_dismiss_confirm_menu_scrolled;
                }
                g_formation_dismiss_menu_data = menu;
                menu[1].selected_index = 0;
                menu[3].selected_index = 0;
                menu[2].text_id = text_id;
            }
            g_formation_dismiss_prompt_running = 1;
        }
        g_help_message_text_id = -1;
    }

    if (g_formation_dismiss_step == 2) {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        g_formation_dismiss_prompt_running = running;
        if (running != 0) {
            return;
        }
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        return;
    }

    running = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_formation_dismiss_menu_data);
    step = g_formation_dismiss_step;
    g_formation_dismiss_prompt_running = running;
    if (step != 0) {
        /* Step 1 queues the answer's sound once; 0x64 then keeps the step nonzero without repeating it. */
        if (thread_is_running(3) != 0) {
            step = g_formation_dismiss_step;
            if (step == 1) {
                selection = g_screen_selection_results[10];
                g_sound_suppressed = step;
                if (selection == 0) {
                    g_sound_queued_effect_id = SOUND_SFX_DISMISS;
                } else if (selection == step) {
                    g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                }
                /* A default assignment plus a conditional override, not an
                 * if/else over the two stores: the target puts `li v1,1` in the
                 * branch's delay slot and overrides with 0x64 on the
                 * fall-through (`beqz` at 0x3e4). An if/else emits the opposite
                 * branch sense and orders the two `li`s the other way, which
                 * also costs the store's register. */
                step = 1;
                if (g_sound_queued_effect_id != SOUND_SFX_NONE) {
                    step = 0x64;
                }
                g_formation_dismiss_step = step;
            }
        }

        if (g_formation_dismiss_prompt_running != 0) {
            return;
        }
        if (g_screen_selection_results[10] != 0) {
            g_sound_suppressed = 0;
            g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
            return;
        }
        g_formation_dismiss_anim_timer = (g_unit_selected_index << 8) + 1;
        return;
    }

    if (running != 0) {
        return;
    }
    g_sound_suppressed = 0;
    g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
}

/* Add `delta` to the war funds, clamped to 0..99,999,999, and return the new
 * amount. */
s32 shop_adjust_gil(s32 delta) {
    s32 gil = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + delta;

    if (gil < 0) {
        gil = 0;
    }
    if (gil > EVENT_WAR_FUNDS_MAX) {
        gil = EVENT_WAR_FUNDS_MAX;
    }
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, gil);
    return gil;
}

/* Convert inventory copies above the owned-item limit into gil.
 *
 * Equipped copies count toward 99; only inventory quantities are reduced.
 */
void shop_cash_out_excess_items(void) {
    s32 item_id = 0;
    s32 excess;
    s32 price;
    u8* quantity = g_item_quantities;

    for (; item_id < ITEM_ID_END - 1; item_id++) {
        excess = item_get_total_count(item_id);
        if (excess >= 100) {
            price = shop_get_item_half_price(item_id);
            excess -= 99;
            shop_adjust_gil(price * excess);
            *quantity -= excess;
        }
        quantity++;
    }
}
