#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"

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

    g_menu_thread_menu_data = g_battle_menu_idle_action_entries;
    g_menu_current_id = BATTLE_MENU_ID_NONE;
    g_menu_restore_pending = 0;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (command == BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK) {
        value = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if (value >= EVENT_ID_TUTORIAL_FIRST && value <= EVENT_ID_TUTORIAL_LAST) {
            if (option != 2) {
                battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (g_menu_input_mode != MENU_INPUT_PLAYER) {
            if (option != 2) {
                battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (option != 1) {
            if (battle_thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
                main_system_report_stub(7, 3);
                if (option != 2) {
                    battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
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
                        battle_unit_start_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
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
                            battle_unit_start_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                            g_menu_overlay_state = 1;
                            script_start_current_event_thread();
                        } else {
                            g_script_next_event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, value);
                        }
                        return MENU_SYSTEM_DISPATCH_EVENT;
                    }
                    if (option != 2) {
                        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                } else if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
                    if (option != 2) {
                        battle_unit_start_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
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
                        battle_unit_start_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
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
                        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                }
            }
        }
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        return MENU_SYSTEM_DISPATCH_DONE;
    }
    if (enabled == 0) {
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
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
        } else if (g_battle_menu_used_skillset_id == SKILLSET_ID_DEFEND) {
            value = 0x1C;
        }
        battle_menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK) {
        sound_queue_sfx_confirm();
        if (option == 0) {
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else if (option == 1) {
            battle_menu_start_system_function_thread(0);
        } else if (option == 4) {
            battle_menu_start_system_function_thread(0x30);
        } else {
            sound_queue_sfx_invalid();
            battle_menu_start_system_function_thread(option + 0xD);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_WAIT_MENU) {
        if (g_battle_menu_system_function_command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK
            && g_battle_menu_system_function_option == 1) {
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else {
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_WAIT_MENU);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM) {
        value = 0x2F;
        if (option != 5) {
            value = 0xB;
            if (option != 1) {
                if (option == 2 || option == 4) {
                    if (g_battle_menu_used_item_id == 0xFFFE || g_menu_selected_skill_ct == 0) {
                        value = 0x2D;
                    } else {
                        value = 0xC;
                    }
                } else {
                    if (g_battle_menu_used_item_id != 0xFFFE || g_menu_selected_skill_ct == 0) {
                        value = 0x2E;
                    } else {
                        value = 0xD;
                    }
                }
            }
        }
        g_menu_restore_pending = 1;
        battle_menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
        g_battle_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        thread_start(THREAD_SLOT_SYSTEM_FUNCTION, battle_text_run_spell_quote_thread);
        thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, unit_id, g_battle_spell_quote_last_ability_id, 0);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
        text_set_message_duration_frames(0x3C);
        if (g_main_action_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_ARITHMETICKS) {
            g_text_substitution_values[1] = unit->action_command.calculator_multiplier + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[0] = unit->action_command.calculator_type + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[2] = unit->action_command.ability_id + TEXT_ID_ABILITY_NAME_BASE;
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ARITHMETICKS_ANNOUNCEMENT);
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
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ABILITY_ANNOUNCEMENT);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_CLOSE) {
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
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
        battle_menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE) {
        if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else {
            g_battle_system_function_table[BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE].text_id = option;
            g_text_substitution_values[0] = parameter & 0xFF;
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_LEARNED) {
        g_text_substitution_values[0] = option + TEXT_ID_ABILITY_NAME_BASE;
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ABILITY_LEARNED);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE) {
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM) {
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_TRAP_RESULT) {
        battle_menu_start_system_function_thread(option + BATTLE_SYSTEM_FUNCTION_TRAP_RESULT_FIRST);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM) {
        if (main_item_get_total_equipment_quantity(option & 0xFF, 1) == 0x63) {
            main_item_get_data_pointer(option);
            g_text_substitution_values[0] = option;
            g_text_substitution_values[1] = option;
            item = main_item_get_data_pointer(option);
            g_text_substitution_values[2] = item->price >> 2;
            script_set_variable(
                EVENT_SCRIPT_VAR_WAR_FUNDS, (item->price >> 2) + script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS));
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM_SOLD);
        } else {
            main_item_get_data_pointer(option);
            g_text_substitution_values[0] = option;
            g_main_item_quantities[option]++;
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT) {
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM_SILENT);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_POST_ACTION_RESULT) {
        entry = &g_battle_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT];
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
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_GIL) {
            g_text_substitution_values[0] = parameter < 0 ? -parameter : parameter;
            g_battle_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508F;
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_EXP) {
            g_text_substitution_values[0] = parameter & 0x7F;
            g_battle_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508E;
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_STOLEN || option == ACTION_DISPLAY_IMAGE_BROKEN) {
            if (parameter & 0x8000) {
                g_text_substitution_values[0] = (parameter & 0xFF) + TEXT_ID_ITEM_NAME_BASE;
                g_battle_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x509B;
            } else {
                g_text_substitution_values[0] = parameter + TEXT_ID_ITEM_NAME_BASE;
                g_battle_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = option + 0x5074;
            }
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else {
            battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        }
    } else {
        main_system_report_stub(7, 5);
        battle_menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        g_battle_menu_system_function_command = command;
        g_battle_menu_system_function_option = option;
        return MENU_SYSTEM_DISPATCH_UNKNOWN_COMMAND;
    }
    g_battle_menu_system_function_command = command;
    g_battle_menu_system_function_option = option;
    return MENU_SYSTEM_DISPATCH_DONE;
}
