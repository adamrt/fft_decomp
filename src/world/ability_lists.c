/* Ability category checks, the learn and view ability lists, unit ordering, text entry waits, the dismiss unit step and
 * gil adjustment. */

#include "fft/data_ability.h"
#include "fft/data_item.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/main_item.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_item.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "fft/world_text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 world_ability_run_view_list(void);

/* Ability-list menu-script callback 13: 1 while the list shows the Action category. The
 * row argument is ignored. */
static s32 world_ability_is_category_action(s32 unused_row) {
    return g_world_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}

/* Ability-list menu-script callback 12: 1 while the list shows the Reaction category. */
static s32 world_ability_is_category_reaction(s32 unused_row) {
    return g_world_ability_category == MENU_ABILITY_CATEGORY_REACTION;
}

/* Ability-list menu-script callback 11: 1 while the list shows the Support category. */
static s32 world_ability_is_category_support(s32 unused_row) {
    return g_world_ability_category == MENU_ABILITY_CATEGORY_SUPPORT;
}

/* Ability-list menu-script callback 10: 1 while the list shows the Movement category. */
static s32 world_ability_is_category_movement(s32 unused_row) {
    return g_world_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}

/* Report whether ability entry `index` counts as already learned: the learned
 * flag in the learn menu (WORLD_FORMATION_MENU_ABILITY_LEARN), otherwise the
 * inverse of the disabled flag. */
static s32 world_menu_is_learn_ability_learned(s32 index) {
    if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY_LEARN) {
        return g_menu_ability_entries[index] & ABILITY_LIST_ENTRY_LEARNED;
    }
    return (g_menu_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}

/*
 * Run the learn-abilities list for the job chosen in the learn-menu job list.
 *
 * Learn-menu twin of world_ability_run_view_list. Confirming an unlearned
 * ability opens the confirmation menu thread; when it closes with choice 0
 * the ability is learned and its JP cost deducted from the unit's JP in the
 * job. Category tabs keep their own cursor (selection records 10-13). Cancel
 * returns MENU_JOB_LIST_STEP_JOBS (back to the job list), otherwise
 * MENU_JOB_LIST_STEP_ABILITIES.
 */
s32 world_ability_run_learn_list(void) {
    s32 unused[160]; /* never referenced; reproduces the target's frame */
    RECT rect;
    s16 x;
    s16 y;
    s32 category;
    u8 running;
    s32 input;
    s32 mode;
    s16 index;

    if (g_world_learn_ability_list_open == 0) {
        g_world_menu_script_callbacks[0].value = world_job_get_cached_level;
        g_world_menu_script_callbacks[1].value = world_job_get_cached_total_jp;
        g_world_menu_script_callbacks[2].value = world_job_get_cached_level_jp_requirement;
        g_world_menu_script_callbacks[3].value = world_job_get_selected_unit_job_points_entry;
        g_world_menu_script_callbacks[4].value = menu_ability_is_non_action_learned;
        g_world_menu_script_callbacks[5].value = world_job_get_cached_skillset_empty_flag;
        g_world_menu_script_callbacks[6].value = world_menu_is_learn_ability_learned;
        g_world_menu_script_callbacks[7].value = menu_ability_get_mp_cost_display_value;
        g_world_menu_script_callbacks[8].value = menu_ability_get_ct_display_value;
        g_world_menu_script_callbacks[9].value = menu_ability_get_jp_cost_display_value;
        g_world_menu_script_callbacks[10].value = world_ability_is_category_movement;
        g_world_menu_script_callbacks[11].value = world_ability_is_category_support;
        g_world_menu_script_callbacks[12].value = world_ability_is_category_reaction;
        g_world_menu_script_callbacks[13].value = world_ability_is_category_action;
        g_world_menu_script_callbacks[14].value = menu_ability_is_non_action_unlearned;
        g_world_learn_confirm_prompt_state = 0;
        g_world_menu_script_callbacks[15].value = menu_ability_is_action;
        menu_clear_selection_record(10);
        menu_clear_selection_record(11);
        menu_clear_selection_record(12);
        menu_clear_selection_record(13);
        g_world_ability_category = menu_update_wrapped_horizontal_selection(4, WORLD_MENU_CURSOR_ABILITY_TAB, 0);
        ability_build_list(g_unit_selected_index, g_world_learn_selected_job, g_world_ability_category,
            g_menu_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
        menu_init_scrollable_list_core(g_menu_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_world_learn_job_name_list[0] = g_world_learn_selected_job;
        world_text_render_id_list_to_image_rows(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_world_learn_job_name_list, &rect, 0);
        g_world_learn_ability_list_open = 1;
        g_world_formation_cursor_ot_override = 0x3c;
    }
    if (g_menu_list_entry_count == 0) {
        g_menu_help_message_text_id = -1;
    } else {
        g_menu_help_message_text_id = (g_menu_ability_entries[g_menu_list_selected_index] & ABILITY_LIST_ENTRY_ID_MASK)
            + HELP_TEXT_ID_ABILITY_BASE;
    }
    if (g_world_learn_confirm_prompt_state == 0) {
        if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) && g_menu_list_entry_count != 0) {
            index = g_menu_list_selected_index;
            /* DISABLED in the learn list: the unit cannot pay the JP. */
            if (g_menu_ability_entries[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
                world_text_show_message_and_play_sound(0xc009, MAIN_SFX_ERROR_MESSAGE);
            } else if (world_menu_is_learn_ability_learned(index) != 0) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_learn_confirm_prompt_state
                    = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_learn_confirm_menu);
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                g_world_learn_confirm_submenu.selected_index = 0;
                g_sound_suppressed = 1;
            }
        } else if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
            g_world_learn_ability_list_open = 0;
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_cursor_ot_override = 0;
            return MENU_JOB_LIST_STEP_JOBS;
        }
        category = menu_update_horizontal_selection_and_mark_change(
            4, WORLD_MENU_CURSOR_ABILITY_TAB, g_menu_input_newly_pressed, MAIN_SFX_PAGE_SWITCH);
        if (category != g_world_ability_category) {
            menu_set_selection_record(g_world_ability_category + 10, g_menu_list_selected_index,
                g_menu_scroll_base_index, g_menu_ability_entries);
            g_world_ability_category = category;
            ability_build_list(g_unit_selected_index, g_world_learn_selected_job, g_world_ability_category,
                g_menu_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            world_menu_get_selection_record(g_world_ability_category + 10, &x, &y, g_menu_ability_entries);
            menu_init_scrollable_list(
                g_menu_ability_entries, x, y, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_set_window_scale_step(10);
        }
    } else {
        g_world_learn_confirm_prompt_state
            = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_learn_confirm_menu);
        if (g_world_learn_confirm_prompt_state == 0) {
            if (g_world_menu_selection_results[5] == 0) {
                world_ability_learn(g_unit_selected_index, g_world_learn_selected_job,
                    g_menu_ability_entries[g_menu_list_selected_index]);
                /* `category` doubles as the JP cost; a separate local changes the register allocation. */
                category = g_main_ability_data[g_menu_ability_entries[g_menu_list_selected_index]].jp_cost;
                g_world_unit_pointers[g_unit_selected_index]
                    ->job_points.current.values[g_world_learn_selected_job_index] -= category;
                main_sound_play_sfx_find_channel(MAIN_SFX_ABILITY_LEARNED);
            } else if (g_world_menu_selection_results[5] == 1) {
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
            g_sound_suppressed = 0;
            ability_build_list(g_unit_selected_index, g_world_learn_selected_job, g_world_ability_category,
                g_menu_ability_entries, ABILITY_LIST_MODE_LEARN_MENU);
            menu_init_scrollable_list_core(g_menu_ability_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            menu_set_window_scale_step(10);
            world_menu_reset_selection_results();
        }
    }
    running = g_world_learn_confirm_prompt_state;
    input = 0;
    if (running == 0) {
        input = g_menu_input_primary_repeat;
    }
    mode = 0;
    if (g_menu_input_lock_mode != 0 || running != 0) {
        mode = 1;
    }
    menu_run_script_with_palette_mode(&g_world_ability_list_script, input, mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_world_menu_window_tpage, g_world_menu_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}

/* The formation screen's view-abilities menu, run each frame (sibling of world_menu_run_learn):
 * sets up on first entry, runs the job list (MENU_JOB_LIST_STEP_JOBS) or the chosen job's
 * ability list (MENU_JOB_LIST_STEP_ABILITIES), and returns to the unit grid on
 * MENU_JOB_LIST_STEP_CLOSE. */
void world_menu_run_view_abilities(void) {
    if (g_world_view_abilities_initialized == 0) {
        g_world_view_abilities_initialized = 1;
        g_world_formation_unit_cycle_mode = 1;
        g_world_view_abilities_step = MENU_JOB_LIST_STEP_JOBS;
        g_menu_status_display_thread_params.style = 1;
        g_menu_numeric_panel_params.style = 1;
        g_world_equipment_ability_panel_thread_params.style = 1;
        world_menu_init_cursor_position(WORLD_MENU_CURSOR_ABILITY_TAB, 0);
        menu_clear_selection_records();
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (g_world_view_abilities_step == MENU_JOB_LIST_STEP_JOBS) {
        g_world_view_abilities_step = world_menu_run_learn_job_list();
    } else if (g_world_view_abilities_step == MENU_JOB_LIST_STEP_ABILITIES) {
        g_world_view_abilities_step = world_ability_run_view_list();
    }
    if (g_world_view_abilities_step == MENU_JOB_LIST_STEP_CLOSE) {
        g_menu_numeric_panel_params.style = 0;
        g_world_equipment_ability_panel_thread_params.style = 0;
        g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        g_world_formation_unit_cycle_mode = 2;
        menu_input_clear_state();
        g_world_input_secondary_repeat_latched = 0;
        g_menu_status_display_thread_params.style = 0;
        g_world_view_abilities_initialized = 0;
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
static s32 world_ability_run_view_list(void) {
    s32 unused[160]; /* never referenced; reproduces the target's frame */
    RECT rect;
    s16 x;
    s16 y;
    u16 entry;
    s32 category;

    if (g_world_ability_view_list_initialized == 0) {
        g_world_menu_script_callbacks[0].value = world_job_get_cached_level;
        g_world_menu_script_callbacks[1].value = world_job_get_cached_total_jp;
        g_world_menu_script_callbacks[2].value = world_job_get_cached_level_jp_requirement;
        g_world_menu_script_callbacks[3].value = world_job_get_selected_unit_job_points_entry;
        g_world_menu_script_callbacks[4].value = menu_ability_is_non_action_learned;
        g_world_menu_script_callbacks[5].value = world_job_get_cached_skillset_empty_flag;
        g_world_menu_script_callbacks[6].value = world_menu_is_learn_ability_learned;
        g_world_menu_script_callbacks[7].value = menu_ability_get_mp_cost_display_value;
        g_world_menu_script_callbacks[8].value = menu_ability_get_ct_display_value;
        g_world_menu_script_callbacks[9].value = menu_ability_get_jp_cost_display_value;
        g_world_menu_script_callbacks[10].value = world_ability_is_category_movement;
        g_world_menu_script_callbacks[11].value = world_ability_is_category_support;
        g_world_menu_script_callbacks[12].value = world_ability_is_category_reaction;
        g_world_menu_script_callbacks[13].value = world_ability_is_category_action;
        g_world_menu_script_callbacks[14].value = menu_ability_is_non_action_unlearned;
        g_world_view_abilities_confirm_prompt_state = 0;
        g_world_menu_script_callbacks[15].value = menu_ability_is_action;
        menu_clear_selection_record(10);
        menu_clear_selection_record(11);
        menu_clear_selection_record(12);
        menu_clear_selection_record(13);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_world_view_job_name_list[0] = g_world_learn_selected_job;
        world_text_render_id_list_to_image_rows(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_world_view_job_name_list, &rect, 0);
        g_world_ability_category = menu_update_wrapped_horizontal_selection(4, WORLD_MENU_CURSOR_ABILITY_TAB, 0);
        ability_build_list(g_unit_selected_index, g_world_learn_selected_job, g_world_ability_category,
            g_menu_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        menu_init_scrollable_list_core(g_menu_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        g_world_ability_view_list_initialized = 1;
    }
    if (g_menu_list_entry_count == 0
        || ((entry = g_menu_ability_entries[g_menu_list_selected_index]) & ABILITY_LIST_ENTRY_HIDE_DETAILS)) {
        g_menu_help_message_text_id = -1;
    } else {
        g_menu_help_message_text_id = (entry & ABILITY_LIST_ENTRY_ID_MASK) + HELP_TEXT_ID_ABILITY_BASE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_ability_view_list_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_JOB_LIST_STEP_JOBS;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        g_world_ability_view_list_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    category = menu_update_horizontal_selection_and_mark_change(
        4, WORLD_MENU_CURSOR_ABILITY_TAB, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
    if (category != g_world_ability_category) {
        menu_set_selection_record(g_world_ability_category + 10, g_menu_list_selected_index, g_menu_scroll_base_index,
            g_menu_ability_entries);
        g_world_ability_category = category;
        ability_build_list(g_unit_selected_index, g_world_learn_selected_job, g_world_ability_category,
            g_menu_ability_entries, ABILITY_LIST_MODE_VIEW_ALL);
        world_menu_get_selection_record(g_world_ability_category + 10, &x, &y, g_menu_ability_entries);
        menu_init_scrollable_list(g_menu_ability_entries, x, y, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        menu_set_window_scale_step(10);
    }
    menu_run_script_with_palette_mode(
        &g_world_ability_list_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_world_menu_window_tpage, g_world_menu_window_clut, 0xb);
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
void world_menu_run_order_unit(void) {
    s16 option_ids[12]; /* seven entries plus terminator; sized for the retail frame */
    s32 index;
    s32 selected_entry;

    if (g_world_menu_order_unit_open == 0) {
        menu_load_sort_key_list(0, g_world_menu_order_unit_entries);
        for (index = 0; index < 7; index++) {
            option_ids[index] = g_world_menu_order_unit_entries[index] + 14;
        }
        option_ids[7] = -1;
        text_concatenate_entries(g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, option_ids, 1);
        g_world_formation_unit_banner_enabled = 0;
        g_world_order_unit_saved_browse_enabled = g_world_formation_unit_browse_enabled;
        g_world_formation_unit_browse_enabled = 0;
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
        g_world_menu_order_unit_cursor = 0;
        g_world_menu_order_unit_open = 1;
    }

    if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_order_unit_menu) == 0) {
        g_world_menu_order_unit_open = 0;
        g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        g_world_formation_unit_browse_enabled = g_world_order_unit_saved_browse_enabled;
    }

    g_menu_help_message_text_id = g_world_menu_order_unit_entries[g_world_menu_order_unit_cursor] + 0x103f;
    if (g_world_menu_selection_results[9] != -1) {
        /* Move the chosen entry to the front and redraw the options. */
        index = g_world_menu_selection_results[9];
        selected_entry = g_world_menu_order_unit_entries[index];
        index -= 1;
        for (; index >= 0; index--) {
            g_world_menu_order_unit_entries[index + 1] = g_world_menu_order_unit_entries[index];
        }
        g_world_menu_order_unit_entries[0] = selected_entry;
        world_menu_reset_selection_results();
        for (index = 0; index < 7; index++) {
            option_ids[index] = g_world_menu_order_unit_entries[index] + 14;
        }
        option_ids[7] = -1;
        text_concatenate_entries(g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, option_ids, 1);
        g_world_menu_text_redraw_request = 1;
        menu_store_sort_key_list(0, g_world_menu_order_unit_entries);
        world_formation_sort_roster(0);
    }
}

/* Message entry thread whose speaker is the selected formation unit's
 * roster slot. */
void world_menu_text_entry_wait_thread_for_selected_unit(void) {
    /* The retail code reads roster_slot with lhu; keep the byte-exact width. */
    g_world_selected_unit_text_binding.speaker_id = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;
    menu_text_entry_wait_thread();
}

/*
 * Per-frame step of the "dismiss unit" menu
 * (WORLD_FORMATION_MENU_DISMISS_UNIT), called by world_formation_process_frame.
 * Three phases: first entry, which picks the prompt text and menu record;
 * the per-frame poll of the prompt or menu thread; and the dismissal timer.
 *
 * On entry a unit that is away on a proposition only gets message 0xC012.
 * Otherwise g_world_formation_dismiss_step takes
 * world_formation_can_dismiss_unit's answer: 0 shows the unit's refusal line
 * and returns to the grid, nonzero runs the yes/no confirmation. The line
 * comes from the dismiss-reply text (WORLD_TEXT_ID_FORMATION_TEXT_BASE + n): a generic unit (sprite set
 * 0x4B and up) draws one of eight lines for its gender or monster kind with
 * rand(); a special unit uses its sprite set, + 0x17 for the farewell and
 * + 0x61 for the refusal, with sprite set 0x48 sharing 0x49's lines.
 *
 * Confirming plays the dismiss sound and starts a 16-frame timer (the unit
 * index is carried in its high byte); when it runs out every
 * g_world_formation_dismiss_entries row matching the unit's sprite set has
 * its script variable set to 0xC and the unit is removed.
 */
void world_formation_run_dismiss_unit_menu_step(void) {
    world_unit_t* unit;
    menu_entry_t* menu;
    s32 i;
    s32 value;
    s32 sprite;
    s32 flags;
    s32 step;
    /* The sum `value + WORLD_TEXT_ID_FORMATION_TEXT_BASE` is its own variable: the target's a0 carries the
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
    if (g_world_formation_dismiss_anim_timer != 0) {
        g_world_formation_dismiss_anim_timer++;
        if ((g_world_formation_dismiss_anim_timer & 0xFF) < 0x11) {
            return;
        }
        for (i = 0; g_world_formation_dismiss_entries[i].sprite_set != -1; i++) {
            if (g_world_unit_pointers[g_unit_selected_index]->sprite_set
                == g_world_formation_dismiss_entries[i].sprite_set) {
                script_set_variable(g_world_formation_dismiss_entries[i].variable_id, 0xC);
            }
        }
        world_formation_dismiss_unit(g_unit_selected_index);
        g_sound_suppressed = 0;
        g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        return;
    }

    if (g_world_dismiss_unit_prompt_running == 0) {
        sprite = g_unit_selected_index;
        if (g_world_unit_pointers[sprite]->proposition_status != 0) {
            world_text_show_message_and_play_sound(0xC012, MAIN_SFX_ERROR_MESSAGE);
            g_world_formation_dismiss_step = 2;
            g_world_dismiss_unit_prompt_running = 1;
        } else {
            g_world_formation_dismiss_step = world_formation_can_dismiss_unit(sprite);
            world_text_bind_section_data(WORLD_TEXT_DATA_DISMISS_UNIT_REPLIES);
            if (g_world_formation_unit_browse_enabled != 0) {
                g_world_formation_unit_cycle_mode = 1;
            }
            g_text_substitution_values[0] = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;

            /* Generic units (sprite set 0x4B and up) draw a random line for their kind; special units
             * have their own farewell and refusal. */
            unit = g_world_unit_pointers[g_unit_selected_index];
            sprite = unit->sprite_set;
            if (sprite >= 0x4B) {
                flags = unit->gender_flags;
                if (flags & UNIT_FLAG_MALE) {
                    value = rand() % 8;
                } else if (flags & UNIT_FLAG_FEMALE) {
                    value = rand() % 8 + 8;
                } else {
                    value = rand() % 8 + 0x10;
                }
            } else {
                if (sprite == 0x48) {
                    sprite = 0x49;
                }
                if (g_world_formation_dismiss_step != 0) {
                    value = sprite + 0x17;
                } else {
                    value = sprite + 0x61;
                }
            }
            text_id = value + WORLD_TEXT_ID_FORMATION_TEXT_BASE;

            /* A refusal is a plain prompt; a dismissable unit gets the yes/no menu with its farewell. */
            if (g_world_formation_dismiss_step == 0) {
                g_sound_suppressed = 1;
                g_world_formation_dismiss_menu_data = &g_world_formation_dismiss_prompt;
                if (g_world_formation_unit_banner_at_bottom != 0) {
                    value = 0x19;
                } else {
                    value = 0x1A;
                }
                g_world_formation_dismiss_prompt_kind = value;
                g_world_formation_dismiss_prompt_text_id = text_id;
            } else {
                menu = g_world_formation_dismiss_confirm_menu;
                if (g_world_formation_unit_banner_at_bottom != 0) {
                    menu = g_world_formation_dismiss_confirm_menu_scrolled;
                }
                g_world_formation_dismiss_menu_data = menu;
                menu[1].selected_index = 0;
                menu[3].selected_index = 0;
                menu[2].text_id = text_id;
            }
            g_world_dismiss_unit_prompt_running = 1;
        }
        g_menu_help_message_text_id = -1;
    }

    if (g_world_formation_dismiss_step == 2) {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        g_world_dismiss_unit_prompt_running = running;
        if (running != 0) {
            return;
        }
        g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        return;
    }

    running = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_formation_dismiss_menu_data);
    step = g_world_formation_dismiss_step;
    g_world_dismiss_unit_prompt_running = running;
    if (step != 0) {
        /* Step 1 queues the answer's sound once; 0x64 then keeps the step nonzero without repeating it. */
        if (thread_is_running(3) != 0) {
            step = g_world_formation_dismiss_step;
            if (step == 1) {
                selection = g_world_menu_selection_results[10];
                g_sound_suppressed = step;
                if (selection == 0) {
                    g_sound_queued_effect_id = MAIN_SFX_DISMISS;
                } else if (selection == step) {
                    g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                }
                /* A default assignment plus a conditional override, not an
                 * if/else over the two stores: the target puts `li v1,1` in the
                 * branch's delay slot and overrides with 0x64 on the
                 * fall-through (`beqz` at 0x3e4). An if/else emits the opposite
                 * branch sense and orders the two `li`s the other way, which
                 * also costs the store's register. */
                step = 1;
                if (g_sound_queued_effect_id != MAIN_SFX_NONE) {
                    step = 0x64;
                }
                g_world_formation_dismiss_step = step;
            }
        }

        if (g_world_dismiss_unit_prompt_running != 0) {
            return;
        }
        if (g_world_menu_selection_results[10] != 0) {
            g_sound_suppressed = 0;
            g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
            return;
        }
        g_world_formation_dismiss_anim_timer = (g_unit_selected_index << 8) + 1;
        return;
    }

    if (running != 0) {
        return;
    }
    g_sound_suppressed = 0;
    g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
}

/* Add `delta` to the war funds, clamped to 0..99,999,999, and return the new
 * amount. */
s32 world_shop_adjust_gil(s32 delta) {
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
void world_item_cash_out_excess_inventory(void) {
    s32 item_id = 0;
    s32 excess;
    s32 price;
    u8* quantity = g_main_item_quantities;

    for (; item_id < ITEM_ID_END - 1; item_id++) {
        excess = item_get_total_count(item_id);
        if (excess >= 100) {
            price = world_item_get_half_price(item_id);
            excess -= 99;
            world_shop_adjust_gil(price * excess);
            *quantity -= excess;
        }
        quantity++;
    }
}
