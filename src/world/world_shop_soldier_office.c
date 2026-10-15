/* Soldier office hire, rename and fee steps. */

#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Fur shop version of the equip-candidate step: a return to the buy list
 * goes to the fur shop's buy list. */
void world_shop_run_fur_equip_purchase_step(void) {
    world_shop_run_equip_candidate_step();
    if (g_world_shop_menu_step == WORLD_SHOP_STEP_BUY_LIST)
        g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
}

/* Soldier Office hire step: runs the hire menu and charges for the new
 * unit's starting equipment. */
void world_shop_run_soldier_office_hire_menu_step(void) {
    unit_stats_t unit;
    s32 i;
    s32 result;
    s32 slot;

    if (g_world_shop_hire_menu_initialized == 0) {
        g_world_shop_hire_confirm_menu.selected_index = 0;
        g_world_shop_hire_fee_confirm_menu.selected_index = 0;
        g_world_shop_hire_result_shown = 0;
        g_world_shop_hire_menu_initialized = 1;
        g_menu_status_display_thread_params.y = 0;
        D_801CD0EC = 0;
        D_801CD0F4 = 0;
        g_world_formation_unit_banner_enabled = 0;
        g_world_shop_party_unit_count = 0;
        for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
            result = main_unit_init_job_data(&unit, i, 1);
            if (result == 0 && unit.formation_index < PARTY_GUEST_SLOT_FIRST) {
                g_world_shop_party_unit_count++;
            }
        }
    }
    if (g_world_shop_hire_result_shown != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    } else {
        i = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_menu_thread_data);
        if (i == 0) {
            if (g_world_menu_selection_results[0] == 3 || g_world_menu_selection_results[0] == -1) {
                g_world_shop_hire_menu_initialized = 0;
                g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF825, 0);
                world_item_cash_out_excess_inventory();
            } else if (g_world_menu_selection_results[0] == 2) {
                g_world_formation_unit_count
                    = world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_MONSTERS);
                if (g_world_formation_unit_count != 0) {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_RENAME_SELECT_UNIT;
                    i = 0xF847;
                } else {
                    i = 0xF846;
                    g_world_shop_hire_result_shown = 1;
                }
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, i, 0);
            } else {
                slot = world_formation_generate_unit(g_world_menu_selection_results[0]);
                if (slot >= 0) {
                    g_world_shop_service_fee = 1000;
                    for (; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                        g_world_shop_service_fee += world_item_get_price(g_world_unit_pointers[slot]->equipment[i]);
                    }
                    if (world_shop_adjust_gil(0) < g_world_shop_service_fee) {
                        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF823, 0);
                        main_party_remove_unit(g_world_unit_pointers[slot]->identity.roster_slot);
                        g_world_shop_hire_result_shown = 1;
                    } else {
                        g_unit_selected_index = slot;
                        world_formation_stage_selected_unit();
                        g_world_formation_unit_banner_at_bottom = 0;
                        g_world_formation_unit_banner_at_bottom_latch = 0;
                        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF824, 0);
                        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_PREVIEW;
                    }
                } else {
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF822, 0);
                    g_world_shop_hire_result_shown = 1;
                }
            }
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/*
 * Soldier office step that shows the generated recruit.
 *
 * Any face button dismisses the message box; once it has closed, unit
 * browsing and the status banner are turned on for the preview. A further
 * face button closes the panels and moves on to the hire confirmation
 * (message 0xF826).
 */
void world_shop_run_recruit_unit_preview_step(void) {
    if (g_world_shop_recruit_preview_initialized == 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_unit_banner_enabled = 1;
            g_world_formation_unit_banner_at_bottom = 0;
            g_menu_status_display_thread_params.y = 0;
            g_menu_help_message_text_id = MENU_GRID_HELP_RECRUIT_PREVIEW;
            g_world_shop_recruit_preview_initialized = 1;
        }
        world_shop_run_obtain_gil_menu();
        world_shop_run_soldier_office_fee_menu_script();
        world_menu_run_script_with_callback_suppressed();
    } else if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_CONFIRM;
        g_world_formation_unit_browse_enabled = 0;
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_numeric_editor_thread(0);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF826, 0);
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_world_shop_recruit_preview_initialized = 0;
    }
}

/*
 * Soldier office: confirm hiring the previewed recruit.
 *
 * Selection 1 or -1 declines and removes the recruit from the roster;
 * selection 2 returns to the preview. Any other result shows a message and
 * waits for its thread before going on to the name entry.
 */
void world_shop_run_hire_confirm_step(void) {
    s16 selection;
    if (g_world_shop_hire_confirm_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_shop_hire_confirm_message_wait = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_NAME_ENTRY;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_confirm_menu) == 0) {
        selection = g_world_menu_selection_results[2];
        if (selection == 1 || selection == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF827, 0);
            g_world_shop_hire_menu_initialized = 0;
            main_party_remove_unit(g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
        } else if (selection == 2) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_PREVIEW;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF829, 0);
            g_world_shop_hire_confirm_message_wait = 1;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/* Soldier office: the name entry keyboard for the recruit, then the fee prompt. */
void world_shop_run_hire_name_entry_step(void) {
    if (g_world_shop_hire_name_entry_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_unit_status_thread(0);
        g_world_shop_hire_name_entry_initialized = 1;
    }
    g_world_shop_entered_unit_name
        = world_name_run_entry_keyboard_frame(g_world_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_hire_name_entry_initialized = 0;
        g_text_substitution_values[0] = g_world_shop_service_fee;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82A, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_FEE_CONFIRM;
    }
}

/*
 * Soldier office: confirm the hiring fee.
 *
 * Choice 0 pays the fee and stores the recruit's name; 1 or -1 removes the
 * recruit from the roster. Other choices return to the name entry.
 */
void world_shop_run_hire_fee_confirm_step(void) {
    s16 choice;
    g_menu_help_message_text_id = 0;
    if (g_world_shop_hire_fee_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
            g_world_shop_hire_fee_message_wait = 0;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_fee_confirm_menu) == 0) {
        choice = g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT];
        if (choice == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf82b, 0);
            world_shop_adjust_gil(-g_world_shop_service_fee);
            world_formation_set_unit_name(g_unit_selected_index, g_world_shop_entered_unit_name);
            g_world_shop_hire_fee_message_wait = 1;
        } else if (choice == 1 || choice == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf827, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
            main_party_remove_unit(g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_world_shop_hire_menu_initialized = 0;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_hire_fee_confirm_menu.selected_index = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_NAME_ENTRY;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/* Return to the soldier office menu with message 0xf821, redrawing the gil,
 * fee and roster-count displays for this frame. */
void world_shop_reopen_soldier_office_menu_step(void) {
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf821, 0);
    g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
    g_world_shop_hire_menu_initialized = 0;
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/* Menu callback: no marker (-1) for any unit. */
s32 world_shop_rename_unit_get_unit_marker(s32 unused_unit_index) {
    return -1;
}

/*
 * Menu step for the rename-unit prompt: on the first frame either leaves
 * the screen when no face button is held, or arms the prompt. Confirm
 * opens the name entry (mode 1 when a unit is already renamed) unless the
 * unit has Reis's dragon identity, which refuses with message 0x1B; cancel backs
 * out with message 0x19.
 */
void world_shop_run_rename_unit_prompt_step(void) {
    s32 mode;
    s32 input;
    s32 text_id;

    if (g_world_shop_rename_prompt_initialized == 0) {
        if (!(g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE))) {
            world_shop_run_obtain_gil_menu();
            world_shop_run_soldier_office_fee_menu_script();
            world_menu_run_script_with_callback_suppressed();
            return;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_rename_error_shown = 0;
        g_world_shop_rename_prompt_initialized = 1;
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_background_visible = 0;
    }
    if (g_world_formation_unit_banner_enabled != 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_world_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_menu_help_message_text_id = text_id;
    } else {
        g_menu_help_message_text_id = 0;
    }
    world_formation_update_and_draw_unit_grid(
        1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode, world_shop_rename_unit_get_unit_marker, 0);
    if (g_world_shop_rename_error_shown != 0) {
        g_menu_help_message_text_id = -1;
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_rename_error_shown = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0 && world_formation_is_unit_list_scroll_needed() == 0) {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CIRCLE) {
            if (g_world_unit_pointers[g_unit_selected_index]->sprite_set == CHARACTER_IDENTITY_REIS_DRAGON) {
                if (g_world_formation_unit_banner_at_bottom != 0) {
                    mode = 1;
                } else {
                    mode = 2;
                }
                thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, mode | 0x18, 0xF848, 0);
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
                g_world_shop_rename_error_shown = 1;
            } else {
                g_world_shop_menu_step = WORLD_SHOP_STEP_RENAME_NAME_ENTRY;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                g_world_formation_unit_banner_enabled = 0;
                g_world_shop_rename_prompt_initialized = 0;
            }
        } else if (input & PSX_PAD_CROSS) {
            g_world_shop_background_visible = 1;
            g_world_shop_rename_prompt_initialized = 0;
            g_world_formation_unit_banner_enabled = 0;
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    }
}

/* Soldier office: the name entry keyboard of the Rename service; sibling of
 * world_shop_run_hire_name_entry_step, which goes on to the fee prompt (0xF82A). */
void world_shop_run_rename_name_entry_step(void) {
    if (g_world_shop_rename_name_entry_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_unit_status_thread(0);
        g_world_shop_rename_name_entry_initialized = 1;
    }
    world_formation_update_and_draw_unit_grid(
        1, 0, 0, 0, g_menu_input_lock_mode, world_shop_rename_unit_get_unit_marker, 0);
    g_world_shop_entered_unit_name
        = world_name_run_entry_keyboard_frame(g_world_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        world_formation_set_unit_name(g_unit_selected_index, g_world_shop_entered_unit_name);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
        g_world_shop_background_visible = 1;
        g_world_shop_rename_name_entry_initialized = 0;
    }
}

/* Menu callback: the hiring fee for the highlighted row of the hire menu,
 * 1500 for row 0, 1400 for row 1 and 0 otherwise. */
s32 world_shop_get_soldier_office_entry_fee(s32 unused_row) {
    s16 mode = g_world_shop_hire_menu_thread_data.selected_index;

    if (mode == 0) {
        return 0x5DC;
    }
    return (mode == 1) ? 0x578 : 0;
}

/* Run the soldier office's fee display script with
 * world_shop_get_soldier_office_entry_fee installed as menu value callback 0,
 * then restore the previous callback. */
void world_shop_run_soldier_office_fee_menu_script(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_world_menu_script_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = world_shop_get_soldier_office_entry_fee;
    menu_run_script_with_palette_mode_variant(&g_world_shop_soldier_office_fee_menu_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Display the party unit count directly while value callback 0 is disabled. */
void world_menu_run_script_with_callback_suppressed(void) {
    s32 (*previous_callback)(s32);

    previous_callback = g_world_menu_script_callbacks[0].value;
    g_world_menu_script_callbacks[0].value = 0;
    g_world_shop_roster_count_menu_script.unit_count.value = g_world_shop_party_unit_count;
    /* The serialized script prefix shares the bytecode interpreter's instruction view. */
    menu_run_script_with_palette_mode_variant((menu_instruction_t*)&g_world_shop_roster_count_menu_script, 0, 0);
    g_world_menu_script_callbacks[0].value = previous_callback;
}
