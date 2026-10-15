#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Test membership in the 0xff-terminated skillset exception list. */
s32 text_is_spell_quote_exception_skillset(s32 skillset_id) {
    s32 i;
    s32 found = 0;

    for (i = 0; g_text_spell_quote_exception_skillsets[i] != SKILLSET_ID_NONE; i++) {
        if (g_text_spell_quote_exception_skillsets[i] == skillset_id) {
            found = 1;
            break;
        }
    }
    return found;
}

/**
 * Resolve the pending action-menu command, option, and item selection.
 *
 * Remaps menu-specific values, publishes the resolved selection, and returns
 * the command, option, and item packed into a single value.
 */
s32 menu_resolve_selection(void) {
    s32 command;
    s32 option;
    u32 item;
    s32 index;
    s32 i;
    s32 menu;

    command = (u8)g_menu_pending_selection[0];
    option = (u8)g_menu_pending_selection[1];
    item = (u16)g_menu_pending_selection[2];
    g_menu_selected_command.kind = command;
    index = option;
    if (command != 0x12 && (command & 0xFE) != 0xFE) {
        menu = g_menu_current_id;
        for (i = 0; i < 31; i++) {
            if (menu == g_menu_command_maps[i].menu_id) {
                if (command != 0xFF) {
                    if (menu == BATTLE_MENU_ID_DEAD_UNIT) {
                        command = g_menu_dead_unit_action;
                        /* Item selection updates only the low byte of the 16-bit command field. */
                        *(u8*)&g_menu_selected_action.item_or_equipment = g_menu_dead_unit_result;
                        option = 0xFFFE;
                    } else if (menu == 0x19 || menu >= 0x64) {
                        command = *(g_menu_command_maps[i].map + option + 1);
                        option = 0xFFFE;
                    } else {
                        command = *(g_menu_command_maps[i].map + command + 1);
                    }
                } else {
                    command = g_menu_command_maps[i].map[0];
                }
                break;
            }
        }
        if (i == 31) {
            command = 7;
        }
        if (i < 16) {
            g_menu_resolved_command = command;
        } else if (command == 5 || command == 6) {
            g_menu_selected_action.targeting_type = command;
        }
        if (command == 1) {
            g_menu_restore_pending = 1;
        }
        if (command == 12) {
            if (option < 3) {
                s32 option_offset = (option - 1) * 2;
                s32 adjusted_item = item + 12;

                command = option_offset + adjusted_item;
            } else {
                command = option + 13;
            }
            option = 0xFFFE;
            item = 0xFFFE;
            g_menu_resolved_command = command;
        }
        g_menu_selected_command.kind = command;
        if ((option & 0xFE) != 0xFE) {
            option = g_menu_skillsets[index];
            if (option == SKILLSET_ID_ATTACK || option == SKILLSET_ID_DEFEND || option == SKILLSET_ID_EQUIP_CHANGE
                || option == SKILLSET_ID_BLANK_04 || option == SKILLSET_ID_ELEMENTAL || option == SKILLSET_ID_JUMP) {
                g_menu_selected_action.ability_id = 0;
                item = 0xFFFE;
            } else if (option == ACTION_MENU_PSEUDO_SKILLSET_ANYTHING && g_menu_skillset_build_result == 0) {
                item = g_menu_anything_ability_id;
            } else {
                item = g_ai_workspace_ptr->ability_list.ids[item] & 0x1FF;
            }
            g_menu_selected_action.skillset_id = option;
            g_menu_used_skillset_id = option;
            if (item != 0xFFFE) {
                if (option == SKILLSET_ID_ITEM || option == SKILLSET_ID_DRAW_OUT || option == SKILLSET_ID_THROW) {
                    *(u8*)&g_menu_selected_action.item_or_equipment = item;
                } else {
                    g_menu_selected_action.ability_id = item;
                }
                g_menu_used_item_id = item;
            }
        }
    }
    return command | ((option & 0xFF) << 8) | (item << 16);
}

/* Wait for menu/event threads, then start the selected system-function entry. */
void menu_run_system_function_thread(void) {
    s32 id;
    s32 menu_id;
    menu_system_function_t* entry;

    g_menu_system_function_thread_busy = 1;
    text_init_section_pointers_variant();
    id = g_thread_contexts[g_thread_current_id].function_parameter_1;
    if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
        id = g_menu_system_function_table[id].alternate_id;
    }
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        while (thread_is_running_asm_variant(THREAD_SLOT_EVENT) != 0) {
            if (g_thread_contexts[THREAD_SLOT_EVENT].task_id == THREAD_TASK_EXECUTE_EVENT) {
                thread_exit_current();
            }
            thread_yield();
        }
    }
    camera_wait_until_idle();
    entry = &g_menu_system_function_table[id];
    menu_id = entry->menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].text_id = entry->text_id;
    g_menu_system_function_row_actions[0] = entry->row_action;
    g_menu_current_id = menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].thread_count = entry->thread_count;
    if (id == 0x1f) {
        thread_start(THREAD_SLOT_MENU, menu_open_option_dead_unit_panel);
        thread_set_parameters(THREAD_SLOT_MENU, 0, g_menu_dead_unit_panel_parameter, 0);
        thread_exit_current();
    }
    if (id != 8) {
        script_pulse_tutorial_wait_value(id);
    }
    thread_start(THREAD_SLOT_MENU, entry->thread_entry);
    thread_set_four_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[entry->menu_entry_index], 0, 0, 0);
    g_menu_system_function_thread_busy = 0;
    thread_exit_current();
}

/* Clear menu selections and start the system-function worker. */
void menu_start_system_function_thread(s32 system_function) {
    menu_clear_entry_selections();
    thread_start(THREAD_SLOT_SYSTEM_FUNCTION, menu_run_system_function_thread);
    thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, system_function, 0, 0);
}

/* Confirm the current menu action, recording the auto-battle mode it selects. */
void menu_confirm_auto_battle_selection_thread(void) {
    unit_stats_t* attacker;
    s32 menu;
    menu_entry_t* parameter;

    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
    attacker = menu_get_selected_unit_stats();
    menu = g_menu_thread_menu_data[BATTLE_MENU_ID_AUTO_BATTLE].selected_index;
    if (menu == 3) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_FADING_LIFE;
    } else if (menu == 4) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_RETREAT;
    }
    g_menu_set_auto_battle_row_actions = -3;
    menu_confirm_action_silently(parameter);
    thread_exit_current();
}

/* Store a unit's auto-battle target together with the resolved menu command as
 * its auto-battle setting. */
void menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id) {
    unit_stats_t* stats = battle_unit_get_stats_from_battle_id(battle_id);
    stats->auto_battle_target = target_id;
    stats->auto_battle_setting = g_menu_resolved_command;
}

/* Chooses whether an ability announcement shows a spell quote.
 *
 * Each scene binds its own quote history and menu dispatcher. Unknown
 * skillsets (not in the list and not the unit's flagged primary/secondary)
 * get quote command 5 unless the ability repeats the last quote
 * (g_text_spell_quote_last_ability_id), the unit is a Calculator or Mime, the ability is >= 0x170 or
 * lacks the spell-quote flag, status blocks it, options disable it, or
 * EVENT_SCRIPT_VAR_RANDOM_VALUE is 4 or more. Everything else uses command 7. */
void text_select_ability_announcement(action_command_t* action, s32 unit_id, s32 enabled) {
    ability_data_t* ability;
    main_ability_secondary_pointer_t secondary;
    unit_stats_t* unit;
    s32 known;

    main_ability_calculate_pointers_and_type(action->ability_id & ABILITY_ID_MASK, &ability, &secondary);
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (action->skillset_id != 0) {
        known = text_is_spell_quote_exception_skillset(action->skillset_id);
    } else {
        known = 1;
    }
    if (action->skillset_id == unit->primary_skillset
        && (unit->position.spell_quote_skillsets & UNIT_SPELL_QUOTE_SUPPRESS_PRIMARY)) {
        known = 1;
    }
    if (action->skillset_id == unit->secondary_skillset
        && (unit->position.spell_quote_skillsets & UNIT_SPELL_QUOTE_SUPPRESS_SECONDARY)) {
        known = 1;
    }
    if (known == 0 && g_text_spell_quote_last_ability_id != (action->ability_id & ABILITY_ID_MASK)
        && unit->job_id != JOB_ID_CALCULATOR && unit->job_id != JOB_ID_MIME
        && action->ability_id < ABILITY_ID_ITEM_FIRST
        && (secondary.range->flags_3 & ABILITY_SECONDARY_FLAG_3_SPELL_QUOTE)
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            & (STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        && g_main_game_options.fields.ability_names == GAME_OPTION_ON
        && (script_get_variable(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 0xF) < 4) {
        g_text_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        menu_dispatch_system_function(BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE, 0, unit_id, 0, enabled, action);
        return;
    }
    if (action->skillset_id != 0) {
        known = text_is_spell_quote_exception_skillset(action->skillset_id);
    } else {
        known = 0;
    }
    if ((known == 0 && (ability->type_flags & ABILITY_TYPE_FLAG_SHOW_NAME)
            && g_main_game_options.fields.ability_names == GAME_OPTION_ON)
        || action->skillset_id == SKILLSET_ID_ITEM || action->skillset_id == SKILLSET_ID_THROW) {
        menu_dispatch_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, enabled, action);
    } else {
        menu_dispatch_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
    }
}

/* Run a system function (battle_menu_system_command_e) for unit_id, looking up
 * the unit's command action first except for SCENARIO_CHECK and CLOSE.
 *
 * SPELL_QUOTE goes to text_select_ability_announcement, a void routine whose
 * leftover v0 is returned. ABILITY_ANNOUNCEMENT drops option, parameter and
 * enabled. Everything else returns menu_dispatch_system_function's result;
 * for SCENARIO_CHECK and CLOSE the action pointer passed on is unset. */
s32 menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled) {
    action_command_t* action;

    if (type != BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK && type != BATTLE_MENU_SYSTEM_COMMAND_CLOSE) {
        action = battle_unit_get_runtime_action_command_by_battle_id(unit_id);
        if (type == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
            /* The target returns the void callee's leftover $v0. */
            return ((s32 (*)(action_command_t*, s32, s32))text_select_ability_announcement)(action, unit_id, enabled);
        }
        if (type == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
            return menu_dispatch_system_function(
                BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
        }
    }
    return menu_dispatch_system_function(type, option, unit_id, parameter, enabled, action);
}

/* Run battle_unit_generate_crystal_or_treasure for the unit on the main stack,
 * keep its record in g_battle_dead_unit_context and return the treasure item when
 * the result is 4, otherwise -1. */
s32 menu_get_dead_unit_selection(s32 battle_id) {
    unit_stats_t* stats;
    crystal_pickup_result_t* result;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    g_thread_call_target = (void (*)(void))battle_unit_generate_crystal_or_treasure;
    result = (crystal_pickup_result_t*)thread_call_on_main_stack(stats);
    g_menu_dead_unit_context = result;
    if (result->result == -1 || result->result == 0) {
        main_system_report_stub(0x11, 7);
    }
    /* Reload the published context after the call; reusing result changes the frame and registers. */
    if (g_menu_dead_unit_context->result != 4) {
        return -1;
    }
    return g_menu_dead_unit_context->treasure_item;
}

/* Dispatch an action-menu command to its scene system function or event. */
s32 menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, action_command_t* action) {
    unit_stats_t* unit;
    s32 value;
    s32 count;
    s32 i;
    item_data_t* item;
    menu_system_function_t* entry;
    s32 flags;
    u8 item_id;

    g_menu_thread_menu_data = g_menu_idle_action_entries;
    g_menu_current_id = BATTLE_MENU_ID_NONE;
    g_menu_restore_pending = 0;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (command == BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK) {
        value = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if (value >= EVENT_ID_TUTORIAL_FIRST && value <= EVENT_ID_TUTORIAL_LAST) {
            if (option != 2) {
                menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (g_menu_input_mode != MENU_INPUT_PLAYER) {
            if (option != 2) {
                menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (option != 1) {
            if (thread_is_running_asm_variant(THREAD_SLOT_EVENT) != 0) {
                main_system_report_stub(7, 3);
                if (option != 2) {
                    menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                }
                return MENU_SYSTEM_DISPATCH_DONE;
            } else {
                count = 0;
                g_script_condition_unit_battle_id = unit_id;
                for (i = 0; i < 3; i++) {
                    if (battle_unit_find_slot_by_character_identity(i + 1) == BATTLE_CHARACTER_SLOT_UNAVAILABLE) {
                        count++;
                    }
                }
                if ((action_check_battle_outcome() < 0 || count != 0)
                    && script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0) {
                    if (option != 2) {
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_GAME_OVER);
                        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        g_script_scenario_event_active = 1;
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_GAME_OVER;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                }
                if (g_script_scenario_event_active != 0) {
                    if (script_run_scenario_conditions() != 0) {
                        if (option != 2) {
                            menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                            g_menu_overlay_state = 1;
                            script_start_current_event_thread();
                        } else {
                            g_script_next_event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, value);
                        }
                        return MENU_SYSTEM_DISPATCH_EVENT;
                    }
                    if (option != 2) {
                        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                } else if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
                    if (option != 2) {
                        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_DEBUG_BATTLE_START);
                        } else {
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_BATTLE_START);
                        }
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_BATTLE_START;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                } else if (action_check_battle_outcome() == 0) {
                    if (option != 2) {
                        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_BATTLE_WON);
                        g_script_scenario_event_active = 1;
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_BATTLE_WON;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                } else {
                    if (option != 2) {
                        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                }
            }
        }
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        return MENU_SYSTEM_DISPATCH_DONE;
    }
    if (enabled == 0) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        return MENU_SYSTEM_DISPATCH_DONE;
    }
    if (command == BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT) {
        value = 8;
        if (g_menu_resolved_command == 0 || g_menu_resolved_command == 0x19) {
            value = 7;
            if (option == 2) {
                value = 0x31;
            }
        } else if (g_menu_resolved_command == 1) {
            g_menu_restore_pending = 1;
            value = 6;
            if (option == 1) {
                value = 0x29;
            }
        } else if (g_menu_resolved_command == 0xC) {
            value = 0x14;
        } else if (g_menu_resolved_command == 0xE) {
            value = 0x15;
        } else if (g_menu_used_skillset_id == SKILLSET_ID_DEFEND) {
            value = 0x1C;
        }
        menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK) {
        sound_queue_sfx_confirm();
        if (option == 0) {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else if (option == 1) {
            menu_start_system_function_thread(0);
        } else if (option == 4) {
            menu_start_system_function_thread(0x30);
        } else {
            sound_queue_sfx_invalid();
            menu_start_system_function_thread(option + 0xD);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_WAIT_MENU) {
        if (g_menu_system_function_command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK
            && g_menu_system_function_option == 1) {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_WAIT_MENU);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM) {
        value = 0x2F;
        if (option != 5) {
            value = 0xB;
            if (option != 1) {
                if (option == 2 || option == 4) {
                    if (g_menu_used_item_id == 0xFFFE || g_menu_selected_skill_ct == 0) {
                        value = 0x2D;
                    } else {
                        value = 0xC;
                    }
                } else {
                    if (g_menu_used_item_id != 0xFFFE || g_menu_selected_skill_ct == 0) {
                        value = 0x2E;
                    } else {
                        value = 0xD;
                    }
                }
            }
        }
        g_menu_restore_pending = 1;
        menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
        g_text_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        thread_start(THREAD_SLOT_SYSTEM_FUNCTION, text_run_spell_quote_thread_variant);
        thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, unit_id, g_text_spell_quote_last_ability_id, 0);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
        text_set_message_duration_frames(0x3C);
        if (g_main_action_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_ARITHMETICKS) {
            g_text_substitution_values[1] = unit->action_command.calculator_multiplier + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[0] = unit->action_command.calculator_type + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[2] = unit->action_command.ability_id + TEXT_ID_ABILITY_NAME_BASE;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ARITHMETICKS_ANNOUNCEMENT);
        } else {
            if (action->skillset_id == SKILLSET_ID_ITEM || action->skillset_id == SKILLSET_ID_THROW) {
                item_id = action->item_or_equipment;
                g_text_substitution_values[0] = item_id + TEXT_ID_ITEM_NAME_BASE;
            } else {
                value = action->ability_id & ABILITY_ID_MASK;
                if (value >= ABILITY_ID_SUMMON_MAGIC_MOOGLE && value <= ABILITY_ID_DRAW_OUT_CHIRIJIRADEN) {
                    g_text_substitution_values[0] = value + 0xB7C4;
                } else {
                    g_text_substitution_values[0] = value + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ABILITY_ANNOUNCEMENT);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_CLOSE) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM) {
        value = 8;
        if (option == 1) {
            value = 0x16;
        } else if (option == 2) {
            value = 0x17;
        } else if (option == 3) {
            value = 0x20;
        } else if (g_menu_resolved_command == 0xC) {
            value = 0x1A;
        } else if (g_menu_resolved_command == 0xE) {
            value = 0x1B;
        }
        menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE) {
        if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else {
            g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE].text_id = option;
            g_text_substitution_values[0] = parameter & 0xFF;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_LEARNED) {
        g_text_substitution_values[0] = option + TEXT_ID_ABILITY_NAME_BASE;
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ABILITY_LEARNED);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_TRAP_RESULT) {
        menu_start_system_function_thread(option + BATTLE_SYSTEM_FUNCTION_TRAP_RESULT_FIRST);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM) {
        if (main_item_get_total_equipment_quantity(option & 0xFF, 1) == 0x63) {
            main_item_get_data_pointer(option);
            g_text_substitution_values[0] = option;
            g_text_substitution_values[1] = option;
            item = main_item_get_data_pointer(option);
            g_text_substitution_values[2] = item->price >> 2;
            script_set_variable(
                EVENT_SCRIPT_VAR_WAR_FUNDS, (item->price >> 2) + script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS));
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM_SOLD);
        } else {
            main_item_get_data_pointer(option);
            g_text_substitution_values[0] = option;
            g_main_item_quantities[option]++;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM_SILENT);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_POST_ACTION_RESULT) {
        entry = &g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT];
        flags = parameter;
        if (option >= ACTION_DISPLAY_IMAGE_CT && option <= ACTION_DISPLAY_IMAGE_LEVEL) {
            g_text_substitution_values[0] = unit_id;
            g_text_substitution_values[1] = option + 0x506B;
            if (option == ACTION_DISPLAY_IMAGE_LEVEL) {
                flags = 1;
                if (parameter & ACTION_STAT_CHANGE_INCREASE) {
                    flags = 1 | ACTION_STAT_CHANGE_INCREASE;
                }
            }
            g_text_substitution_values[2] = flags;
            if (flags & ACTION_STAT_CHANGE_INCREASE) {
                g_text_substitution_values[2] = flags & ACTION_STAT_CHANGE_VALUE_MASK;
                entry->text_id = 0x508C;
            } else {
                entry->text_id = 0x508D;
            }
            if (option == ACTION_DISPLAY_IMAGE_CT) {
                if (flags == 0xFF) {
                    entry->text_id = 0x509A;
                } else if (flags == 0x7F) {
                    entry->text_id = 0x5099;
                }
            }
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_GIL) {
            g_text_substitution_values[0] = parameter < 0 ? -parameter : parameter;
            g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508F;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_EXP) {
            g_text_substitution_values[0] = parameter & 0x7F;
            g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508E;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_STOLEN || option == ACTION_DISPLAY_IMAGE_BROKEN) {
            if (parameter & 0x8000) {
                g_text_substitution_values[0] = (parameter & 0xFF) + TEXT_ID_ITEM_NAME_BASE;
                g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x509B;
            } else {
                g_text_substitution_values[0] = parameter + TEXT_ID_ITEM_NAME_BASE;
                g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = option + 0x5074;
            }
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        }
    } else {
        main_system_report_stub(7, 5);
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        g_menu_system_function_command = command;
        g_menu_system_function_option = option;
        return MENU_SYSTEM_DISPATCH_UNKNOWN_COMMAND;
    }
    g_menu_system_function_command = command;
    g_menu_system_function_option = option;
    return MENU_SYSTEM_DISPATCH_DONE;
}
