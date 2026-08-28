#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/option.h"
#include "fft/script_variables.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"

/*
 * System-function dispatcher called by battle_menu_init_system_function:
 * resets the idle action menu, then maps a command (and its option,
 * parameter and command action) to the system-function thread started by
 * battle_menu_start_system_function_thread, preparing text substitution words
 * and the text id of system function 0x32 where the message needs them.
 * Command 8 may instead select event 0x190-0x193 in script variable 0x27
 * and start the current event thread (return 2); it and any command with
 * enabled == 0 otherwise fall back to system function 8.
 * Returns 1 when a function was started, 0 for an unknown command; handled
 * commands record themselves in g_battle_menu_system_function_command/g_battle_menu_system_function_option.
 *
 * The one-operand keep-alive on unit_id raises its allocation priority so
 * it takes $s6 ahead of action ($s7), as in the target; without it the two
 * callee-saved registers swap. Twin of world_menu_dispatch_system_function.
 */
s32 battle_menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, battle_ai_command_action_t* action) {
    battle_stats_t* unit;
    s32 value;
    s32 count;
    s32 i;
    item_data_t* item;
    battle_system_function_t* entry;
    s32 flags;

    g_battle_menu_thread_menu_data = g_battle_menu_idle_action_entries;
    g_battle_menu_current_id = -1;
    g_battle_menu_restore_pending = 0;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    __asm__("" : : "r"(unit_id));
    if (command == 8) {
        value = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if ((u32)(value - 0x19A) < 0x10) {
            if (option != 2) {
                battle_menu_start_system_function_thread(8);
            }
            return 1;
        } else if (g_battle_menu_input_disabled != 0) {
            if (option != 2) {
                battle_menu_start_system_function_thread(8);
            }
            return 1;
        } else if (option != 1) {
            if (battle_thread_is_running_8014cc94(1) != 0) {
                main_noop_800449f8(7, 3);
                if (option != 2) {
                    battle_menu_start_system_function_thread(8);
                }
                return 1;
            } else {
                count = 0;
                g_battle_script_condition_unit_battle_id = unit_id;
                for (i = 0; i < 3; i++) {
                    if (battle_classify_character_identity_slot(i + 1) == -3) {
                        count++;
                    }
                }
                if ((battle_action_check_battle_outcome() < 0 || count != 0)
                    && battle_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) == 0) {
                    if (option != 2) {
                        battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x190);
                        battle_menu_store_unit_names_and_event_block_data(0, 0xFF, 0xFF);
                        g_option_menu_submenu_state = 1;
                        g_battle_scenario_event_active = 1;
                        battle_script_start_current_event_thread();
                    } else {
                        g_battle_next_event_id = 0x190;
                    }
                    return 2;
                }
                if (g_battle_scenario_event_active != 0) {
                    if (battle_script_run_scenario_conditions() != 0) {
                        if (option != 2) {
                            battle_menu_store_unit_names_and_event_block_data(0, 0xFF, 0xFF);
                            g_option_menu_submenu_state = 1;
                            battle_script_start_current_event_thread();
                        } else {
                            g_battle_next_event_id = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, value);
                        }
                        return 2;
                    }
                    if (option != 2) {
                        battle_menu_start_system_function_thread(8);
                    }
                    return 1;
                } else if (battle_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) != 0) {
                    if (option != 2) {
                        battle_menu_store_unit_names_and_event_block_data(0, 0xFF, 0xFF);
                        g_option_menu_submenu_state = 1;
                        if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
                            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x193);
                        } else {
                            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x191);
                        }
                        battle_script_start_current_event_thread();
                    } else {
                        g_battle_next_event_id = 0x191;
                    }
                    return 2;
                } else if (battle_action_check_battle_outcome() == 0) {
                    if (option != 2) {
                        battle_menu_store_unit_names_and_event_block_data(0, 0xFF, 0xFF);
                        g_option_menu_submenu_state = 1;
                        battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x192);
                        g_battle_scenario_event_active = 1;
                        battle_script_start_current_event_thread();
                    } else {
                        g_battle_next_event_id = 0x192;
                    }
                    return 2;
                } else {
                    if (option != 2) {
                        battle_menu_start_system_function_thread(8);
                    }
                    return 1;
                }
            }
        }
        battle_menu_start_system_function_thread(8);
        return 1;
    }
    if (enabled == 0) {
        battle_menu_start_system_function_thread(8);
        return 1;
    }
    if (command == 1) {
        value = 8;
        if (g_battle_menu_resolved_command == 0 || g_battle_menu_resolved_command == 0x19) {
            value = 7;
            if (option == 2) {
                value = 0x31;
            }
        } else if (g_battle_menu_resolved_command == 1) {
            g_battle_menu_restore_pending = 1;
            value = 6;
            if (option == 1) {
                value = 0x29;
            }
        } else if (g_battle_menu_resolved_command == 0xC) {
            value = 0x14;
        } else if (g_battle_menu_resolved_command == 0xE) {
            value = 0x15;
        } else if (g_battle_menu_used_skillset_id == SKILLSET_ID_DEFEND) {
            value = 0x1C;
        }
        battle_menu_start_system_function_thread(value);
    } else if (command == 2) {
        battle_sound_set_effect_to_confirm();
        if (option == 0) {
            battle_menu_start_system_function_thread(8);
        } else if (option == 1) {
            battle_menu_start_system_function_thread(0);
        } else if (option == 4) {
            battle_menu_start_system_function_thread(0x30);
        } else {
            battle_sound_set_effect_to_invalid();
            battle_menu_start_system_function_thread(option + 0xD);
        }
    } else if (command == 3) {
        if (g_battle_menu_system_function_command == 2 && g_battle_menu_system_function_option == 1) {
            battle_menu_start_system_function_thread(8);
        } else {
            battle_menu_start_system_function_thread(1);
        }
    } else if (command == 4) {
        value = 0x2F;
        if (option != 5) {
            value = 0xB;
            if (option != 1) {
                if (option == 2 || option == 4) {
                    if (g_battle_menu_used_item_id == 0xFFFE || g_battle_menu_selected_skill_target == 0) {
                        value = 0x2D;
                    } else {
                        value = 0xC;
                    }
                } else {
                    if (g_battle_menu_used_item_id != 0xFFFE || g_battle_menu_selected_skill_target == 0) {
                        value = 0x2E;
                    } else {
                        value = 0xD;
                    }
                }
            }
        }
        g_battle_menu_restore_pending = 1;
        battle_menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
        g_battle_spell_quote_last_ability_id = action->ability_id & 0x1FF;
        battle_thread_start(4, battle_text_run_unit_message_thread);
        battle_thread_set_parameters(4, unit_id, g_battle_spell_quote_last_ability_id, 0);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
        battle_text_set_message_duration_frames(0x3C);
        if (g_main_action_menu_types_by_skillset[unit->last_skillset_id] == ACTION_MENU_TYPE_ARITHMETICKS) {
            g_battle_text_substitution_values[1] = (s16)unit->calculator_multiplier_ability + TEXT_ID_ABILITY_NAME_BASE;
            g_battle_text_substitution_values[0] = (s16)unit->calculator_type_ability + TEXT_ID_ABILITY_NAME_BASE;
            g_battle_text_substitution_values[2] = unit->last_ability_id + TEXT_ID_ABILITY_NAME_BASE;
            battle_menu_start_system_function_thread(0x2C);
        } else {
            if (action->skillset == SKILLSET_ID_ITEM || action->skillset == SKILLSET_ID_THROW) {
                g_battle_text_substitution_values[0] = action->item_id + TEXT_ID_ITEM_NAME_BASE;
            } else {
                value = action->ability_id & 0x1FF;
                if ((u32)(value - 0x3C) < 0x1A) {
                    g_battle_text_substitution_values[0] = value + 0xB7C4;
                } else {
                    g_battle_text_substitution_values[0] = value + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            battle_menu_start_system_function_thread(0xA);
        }
    } else if (command == 6) {
        battle_menu_start_system_function_thread(8);
    } else if (command == 9) {
        value = 8;
        if (option == 1) {
            value = 0x16;
        } else if (option == 2) {
            value = 0x17;
        } else if (option == 3) {
            value = 0x20;
        } else if (g_battle_menu_resolved_command == 0xC) {
            value = 0x1A;
        } else if (g_battle_menu_resolved_command == 0xE) {
            value = 0x1B;
        }
        battle_menu_start_system_function_thread(value);
    } else if (command == 0xA) {
        if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
            battle_menu_start_system_function_thread(8);
        } else {
            g_battle_system_function_table[0x1d].text_id = option;
            g_battle_text_substitution_values[0] = parameter & 0xFF;
            battle_menu_start_system_function_thread(0x1D);
        }
    } else if (command == 0xB) {
        g_battle_text_substitution_values[0] = option + TEXT_ID_ABILITY_NAME_BASE;
        battle_menu_start_system_function_thread(0x1E);
    } else if (command == 0xC) {
        battle_menu_start_system_function_thread(0x1F);
    } else if (command == 0xD) {
        battle_menu_start_system_function_thread(0x21);
    } else if (command == 0xE) {
        battle_menu_start_system_function_thread(option + 0x22);
    } else if (command == 0xF) {
        if (get_total_equipment_quantity(option & 0xFF, 1) == 0x63) {
            main_item_get_data_pointer(option);
            g_battle_text_substitution_values[0] = option;
            g_battle_text_substitution_values[1] = option;
            item = main_item_get_data_pointer(option);
            g_battle_text_substitution_values[2] = item->price >> 2;
            battle_script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS,
                (item->price >> 2) + battle_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS));
            battle_menu_start_system_function_thread(0x26);
        } else {
            main_item_get_data_pointer(option);
            g_battle_text_substitution_values[0] = option;
            g_main_item_quantities[option]++;
            battle_menu_start_system_function_thread(0x27);
        }
    } else if (command == 0x10) {
        battle_menu_start_system_function_thread(0x28);
    } else if (command == 0x11) {
        entry = &g_battle_system_function_table[0x32];
        flags = parameter;
        if ((u32)(option - 0x27) < 7) {
            g_battle_text_substitution_values[1] = option + 0x506B;
            g_battle_text_substitution_values[0] = unit_id;
            if (option == 0x2D) {
                flags = 1;
                if (parameter & 0x80) {
                    flags = 0x81;
                }
            }
            g_battle_text_substitution_values[2] = flags;
            if (flags & 0x80) {
                g_battle_text_substitution_values[2] = flags & 0x7F;
                entry->text_id = 0x508C;
            } else {
                entry->text_id = 0x508D;
            }
            if (option == 0x27) {
                if (flags == 0xFF) {
                    entry->text_id = 0x509A;
                } else if (flags == 0x7F) {
                    entry->text_id = 0x5099;
                }
            }
            battle_menu_start_system_function_thread(0x32);
        } else if (option == 0x2E) {
            g_battle_text_substitution_values[0] = parameter < 0 ? -parameter : parameter;
            g_battle_system_function_table[0x32].text_id = 0x508F;
            battle_menu_start_system_function_thread(0x32);
        } else if (option == 0x2F) {
            g_battle_text_substitution_values[0] = parameter & 0x7F;
            g_battle_system_function_table[0x32].text_id = 0x508E;
            battle_menu_start_system_function_thread(0x32);
        } else if ((u32)(option - 0x1C) < 2) {
            if (parameter & 0x8000) {
                g_battle_text_substitution_values[0] = (parameter & 0xFF) + TEXT_ID_ITEM_NAME_BASE;
                g_battle_system_function_table[0x32].text_id = 0x509B;
            } else {
                g_battle_text_substitution_values[0] = parameter + TEXT_ID_ITEM_NAME_BASE;
                g_battle_system_function_table[0x32].text_id = option + 0x5074;
            }
            battle_menu_start_system_function_thread(0x32);
        } else {
            battle_menu_start_system_function_thread(8);
        }
    } else {
        main_noop_800449f8(7, 5);
        battle_menu_start_system_function_thread(8);
        g_battle_menu_system_function_command = command;
        g_battle_menu_system_function_option = option;
        return 0;
    }
    g_battle_menu_system_function_command = command;
    g_battle_menu_system_function_option = option;
    return 1;
}
