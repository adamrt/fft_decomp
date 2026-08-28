#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/script_variables.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"

extern s32 get_total_equipment_quantity(s32 item_id, s32 include_equipped);

/*
 * WORLD twin of the BATTLE system-function dispatcher battle_menu_dispatch_system_function; see
 * that source for the command map. Callers reach it through
 * world_menu_init_system_function and world_text_determine_spell_quote.
 *
 * The one-operand keep-alive on unit_id gives it $s6 ahead of action ($s7),
 * as in the target.
 */
s32 world_menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, battle_ai_command_action_t* action) {
    battle_stats_t* unit;
    s32 value;
    s32 count;
    s32 i;
    item_data_t* item;
    world_system_function_t* entry;
    s32 flags;

    g_world_menu_thread_menu_data = g_world_menu_system_entries;
    g_world_menu_current_id = -1;
    g_world_menu_restore_pending = 0;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    __asm__("" : : "r"(unit_id));
    if (command == 8) {
        value = world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if ((u32)(value - 0x19A) < 0x10) {
            if (option != 2) {
                world_menu_start_system_function_thread(8);
            }
            return 1;
        } else if (g_world_menu_input_disabled != 0) {
            if (option != 2) {
                world_menu_start_system_function_thread(8);
            }
            return 1;
        } else if (option != 1) {
            if (world_thread_is_running_80100164(1) != 0) {
                main_noop_800449f8(7, 3);
                if (option != 2) {
                    world_menu_start_system_function_thread(8);
                }
                return 1;
            } else {
                count = 0;
                g_world_script_acting_unit_id = unit_id;
                for (i = 0; i < 3; i++) {
                    if (battle_classify_character_identity_slot(i + 1) == -3) {
                        count++;
                    }
                }
                if ((battle_action_check_battle_outcome() < 0 || count != 0)
                    && world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) == 0) {
                    if (option != 2) {
                        world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x190);
                        world_unit_start_view_thread(0, 0xFF, 0xFF);
                        g_world_menu_overlay_state = 1;
                        g_world_menu_scenario_event_started = 1;
                        world_script_start_event_from_variables();
                    } else {
                        g_world_finish_operation_event_id = 0x190;
                    }
                    return 2;
                }
                if (g_world_menu_scenario_event_started != 0) {
                    if (world_process_scenario_conditionals() != 0) {
                        if (option != 2) {
                            world_unit_start_view_thread(0, 0xFF, 0xFF);
                            g_world_menu_overlay_state = 1;
                            world_script_start_event_from_variables();
                        } else {
                            g_world_finish_operation_event_id
                                = world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, value);
                        }
                        return 2;
                    }
                    if (option != 2) {
                        world_menu_start_system_function_thread(8);
                    }
                    return 1;
                } else if (world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) != 0) {
                    if (option != 2) {
                        world_unit_start_view_thread(0, 0xFF, 0xFF);
                        g_world_menu_overlay_state = 1;
                        if (world_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
                            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x193);
                        } else {
                            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x191);
                        }
                        world_script_start_event_from_variables();
                    } else {
                        g_world_finish_operation_event_id = 0x191;
                    }
                    return 2;
                } else if (battle_action_check_battle_outcome() == 0) {
                    if (option != 2) {
                        world_unit_start_view_thread(0, 0xFF, 0xFF);
                        g_world_menu_overlay_state = 1;
                        world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x192);
                        g_world_menu_scenario_event_started = 1;
                        world_script_start_event_from_variables();
                    } else {
                        g_world_finish_operation_event_id = 0x192;
                    }
                    return 2;
                } else {
                    if (option != 2) {
                        world_menu_start_system_function_thread(8);
                    }
                    return 1;
                }
            }
        }
        world_menu_start_system_function_thread(8);
        return 1;
    }
    if (enabled == 0) {
        world_menu_start_system_function_thread(8);
        return 1;
    }
    if (command == 1) {
        value = 8;
        if (g_world_menu_resolved_command == 0 || g_world_menu_resolved_command == 0x19) {
            value = 7;
            if (option == 2) {
                value = 0x31;
            }
        } else if (g_world_menu_resolved_command == 1) {
            g_world_menu_restore_pending = 1;
            value = 6;
            if (option == 1) {
                value = 0x29;
            }
        } else if (g_world_menu_resolved_command == 0xC) {
            value = 0x14;
        } else if (g_world_menu_resolved_command == 0xE) {
            value = 0x15;
        } else if (g_world_menu_resolved_skillset == SKILLSET_ID_DEFEND) {
            value = 0x1C;
        }
        world_menu_start_system_function_thread(value);
    } else if (command == 2) {
        world_sound_set_effect_to_confirm();
        if (option == 0) {
            world_menu_start_system_function_thread(8);
        } else if (option == 1) {
            world_menu_start_system_function_thread(0);
        } else if (option == 4) {
            world_menu_start_system_function_thread(0x30);
        } else {
            world_sound_set_effect_to_invalid();
            world_menu_start_system_function_thread(option + 0xD);
        }
    } else if (command == 3) {
        if (g_world_menu_last_dispatched_command == 2 && g_world_menu_last_dispatched_option == 1) {
            world_menu_start_system_function_thread(8);
        } else {
            world_menu_start_system_function_thread(1);
        }
    } else if (command == 4) {
        value = 0x2F;
        if (option != 5) {
            value = 0xB;
            if (option != 1) {
                if (option == 2 || option == 4) {
                    if (g_world_menu_resolved_ability == 0xFFFE || g_world_ability_selected_skill_target == 0) {
                        value = 0x2D;
                    } else {
                        value = 0xC;
                    }
                } else {
                    if (g_world_menu_resolved_ability != 0xFFFE || g_world_ability_selected_skill_target == 0) {
                        value = 0x2E;
                    } else {
                        value = 0xD;
                    }
                }
            }
        }
        g_world_menu_restore_pending = 1;
        world_menu_start_system_function_thread(value);
    } else if (command == 5) {
        g_world_spell_quote_last_ability_id = action->ability_id & 0x1FF;
        world_thread_start(4, world_noop_800ef9d4);
        world_thread_set_parameters(4, unit_id, g_world_spell_quote_last_ability_id, 0);
    } else if (command == 7) {
        world_text_set_message_duration_frames(0x3C);
        if (g_main_action_menu_types_by_skillset[unit->last_skillset_id] == ACTION_MENU_TYPE_ARITHMETICKS) {
            g_world_text_substitution_values[1] = (s16)unit->calculator_multiplier_ability + TEXT_ID_ABILITY_NAME_BASE;
            g_world_text_substitution_values[0] = (s16)unit->calculator_type_ability + TEXT_ID_ABILITY_NAME_BASE;
            g_world_text_substitution_values[2] = unit->last_ability_id + TEXT_ID_ABILITY_NAME_BASE;
            world_menu_start_system_function_thread(0x2C);
        } else {
            if (action->skillset == SKILLSET_ID_ITEM || action->skillset == SKILLSET_ID_THROW) {
                g_world_text_substitution_values[0] = action->item_id + TEXT_ID_ITEM_NAME_BASE;
            } else {
                value = action->ability_id & 0x1FF;
                if ((u32)(value - 0x3C) < 0x1A) {
                    g_world_text_substitution_values[0] = value + 0xB7C4;
                } else {
                    g_world_text_substitution_values[0] = value + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            world_menu_start_system_function_thread(0xA);
        }
    } else if (command == 6) {
        world_menu_start_system_function_thread(8);
    } else if (command == 9) {
        value = 8;
        if (option == 1) {
            value = 0x16;
        } else if (option == 2) {
            value = 0x17;
        } else if (option == 3) {
            value = 0x20;
        } else if (g_world_menu_resolved_command == 0xC) {
            value = 0x1A;
        } else if (g_world_menu_resolved_command == 0xE) {
            value = 0x1B;
        }
        world_menu_start_system_function_thread(value);
    } else if (command == 0xA) {
        if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
            world_menu_start_system_function_thread(8);
        } else {
            g_world_system_function_table[0x1D].text_id = option;
            g_world_text_substitution_values[0] = parameter & 0xFF;
            world_menu_start_system_function_thread(0x1D);
        }
    } else if (command == 0xB) {
        g_world_text_substitution_values[0] = option + TEXT_ID_ABILITY_NAME_BASE;
        world_menu_start_system_function_thread(0x1E);
    } else if (command == 0xC) {
        world_menu_start_system_function_thread(0x1F);
    } else if (command == 0xD) {
        world_menu_start_system_function_thread(0x21);
    } else if (command == 0xE) {
        world_menu_start_system_function_thread(option + 0x22);
    } else if (command == 0xF) {
        if (get_total_equipment_quantity(option & 0xFF, 1) == 0x63) {
            main_item_get_data_pointer(option);
            g_world_text_substitution_values[0] = option;
            g_world_text_substitution_values[1] = option;
            item = main_item_get_data_pointer(option);
            g_world_text_substitution_values[2] = item->price >> 2;
            world_script_set_variable(
                EVENT_SCRIPT_VAR_WAR_FUNDS, (item->price >> 2) + world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS));
            world_menu_start_system_function_thread(0x26);
        } else {
            main_item_get_data_pointer(option);
            g_world_text_substitution_values[0] = option;
            g_main_item_quantities[option]++;
            world_menu_start_system_function_thread(0x27);
        }
    } else if (command == 0x10) {
        world_menu_start_system_function_thread(0x28);
    } else if (command == 0x11) {
        entry = &g_world_system_function_table[0x32];
        flags = parameter;
        if ((u32)(option - 0x27) < 7) {
            g_world_text_substitution_values[1] = option + 0x506B;
            g_world_text_substitution_values[0] = unit_id;
            if (option == 0x2D) {
                flags = 1;
                if (parameter & 0x80) {
                    flags = 0x81;
                }
            }
            g_world_text_substitution_values[2] = flags;
            if (flags & 0x80) {
                g_world_text_substitution_values[2] = flags & 0x7F;
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
            world_menu_start_system_function_thread(0x32);
        } else if (option == 0x2E) {
            g_world_text_substitution_values[0] = parameter < 0 ? -parameter : parameter;
            g_world_system_function_table[0x32].text_id = 0x508F;
            world_menu_start_system_function_thread(0x32);
        } else if (option == 0x2F) {
            g_world_text_substitution_values[0] = parameter & 0x7F;
            g_world_system_function_table[0x32].text_id = 0x508E;
            world_menu_start_system_function_thread(0x32);
        } else if ((u32)(option - 0x1C) < 2) {
            if (parameter & 0x8000) {
                g_world_text_substitution_values[0] = (parameter & 0xFF) + TEXT_ID_ITEM_NAME_BASE;
                g_world_system_function_table[0x32].text_id = 0x509B;
            } else {
                g_world_text_substitution_values[0] = parameter + TEXT_ID_ITEM_NAME_BASE;
                g_world_system_function_table[0x32].text_id = option + 0x5074;
            }
            world_menu_start_system_function_thread(0x32);
        } else {
            world_menu_start_system_function_thread(8);
        }
    } else {
        main_noop_800449f8(7, 5);
        world_menu_start_system_function_thread(8);
        g_world_menu_last_dispatched_command = command;
        g_world_menu_last_dispatched_option = option;
        return 0;
    }
    g_world_menu_last_dispatched_command = command;
    g_world_menu_last_dispatched_option = option;
    return 1;
}
