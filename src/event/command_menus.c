/* Command menu confirmation threads, action restrictions, selection resolution, system functions, scenario conditions,
 * event input routing, the event frame and camera state in script variables. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_gfx_resource.h"
#include "fft/battle_map.h"
#include "fft/battle_map_resource.h"
#include "fft/battle_map_weather.h"
#include "fft/battle_menu.h"
#include "fft/battle_sound.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_encounter.h"
#include "fft/camera.h"
#include "fft/data_ability.h"
#include "fft/data_item.h"
#include "fft/data_job.h"
#include "fft/data_skillset.h"
#include "fft/data_status.h"
#include "fft/event_bunit.h"
#include "fft/event_card.h"
#include "fft/event_equip.h"
#include "fft/event_jobstts.h"
#include "fft/gfx_primitive.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_item.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/menu_command.h"
#include "fft/menu_dead_unit.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_option.h"
#include "fft/menu_overlay.h"
#include "fft/menu_selection.h"
#include "fft/menu_status.h"
#include "fft/menu_text.h"
#include "fft/menu_unit_view.h"
#include "fft/menu_window.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_identity.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"
#ifdef FFT_WORLD
#    include "fft/world_menu.h"
#    include "fft/world_script.h"
#endif

static void camera_load_state_from_script_variables(void);
static void script_process_pending_requests(void);
static void script_route_event_input(void);

static s32 menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, action_command_t* action);
static event_scenario_condition_result_e script_check_scenario_condition(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation);

/* Queue the confirm sound unless a menu restore is replaying selections. */
static void sound_queue_sfx_confirm_unless_restoring(void) {
    if (g_menu_restore_pending == 0) {
        g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    }
}

/* Runs the menu's action handler on a simulated, silent Circle press with
 * option 0 selected, then clears the injected input. */
static void menu_confirm_action_silently(menu_entry_t* menu) {
    g_sound_suppressed = 1;
    g_script_event_input = PSX_PAD_CIRCLE;
    menu_handle_action(menu, 0);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
}

/* Menu-opening thread 6: the Wait menu. */
void menu_wait_thread(void) {
    menu_entry_t* menu = (menu_entry_t*)thread_get_current_parameter_1();

    g_menu_wait_row_actions[0] = -2;
    g_menu_wait_row_actions[1] = -2;
    menu_confirm_action_silently(menu);
    thread_exit_current();
}

/* Thread entry of menu entry 0x22, which confirms the Move command.
 *
 * A unit with Don't Move gets the invalid sound and the linked-entry icon thread
 * (stored row action -1). In a menu whose move is already used (ids 0x0e, 0x14,
 * 0x21, 0x2f, 0x31 and 0x33) only the invalid sound plays. Otherwise the command
 * is confirmed at once, as a silent Circle press. */
void menu_confirm_move_command_thread(void) {
    s16* stored_actions;
    menu_entry_t* entry = (menu_entry_t*)thread_get_current_parameter_1();
    s32 menu;

    stored_actions = &g_menu_confirm_stored_unit_row_actions;
    *stored_actions = -2;
    if (menu_get_selected_unit_stats()->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)]
        & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE)) {
        *stored_actions = -1;
        sound_queue_sfx_invalid();
        menu_icon_linked_entry_thread();
        thread_exit_current();
    }
    menu = g_menu_current_id;
    if (menu == BATTLE_MENU_ID_TURN_MOVED || menu == BATTLE_MENU_ID_TURN_MOVED_ACTED
        || menu == BATTLE_MENU_ID_TURN_NO_ACT_MOVED || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED
        || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED || menu == BATTLE_MENU_ID_RIDER_TURN_NO_ACT_MOVED) {
        sound_queue_sfx_invalid();
        thread_exit_current();
    }
    sound_queue_sfx_confirm_unless_restoring();
    menu_confirm_action_silently(entry);
    thread_exit_current();
}

/*
 * Check action-menu restrictions and show the applicable warning.
 *
 * Menu groups 0x13/0x14 and 0x30/0x31 use the battle eligibility check.
 * Other menus inspect the unit's current status and last action. The packed
 * options can suppress status warnings, but messages 0x1002/0x1003 always
 * stop the current thread after any warning thread finishes.
 */
void menu_check_action_restrictions(void) {
    s32 message = 0;
    unit_stats_t* unit;
    s32 menu;
    s32 result;
    s32 mode;

    unit = menu_get_selected_unit_stats();
    menu = g_menu_current_id;
    if (menu == BATTLE_MENU_ID_TURN_ACTED || menu == BATTLE_MENU_ID_TURN_MOVED_ACTED
        || menu == BATTLE_MENU_ID_RIDER_TURN_ACTED || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED) {
        result = battle_unit_get_action_restriction(unit);
        if (result == BATTLE_UNIT_ACTION_RESTRICTION_NONE) {
            sound_queue_sfx_invalid();
            thread_exit_current();
        }
        if (result == BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT) {
            message = 0x1002;
        }
        if (result == BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER) {
            message = 0x1003;
        }
    } else {
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING)) {
            if (g_main_action_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_CHARGE) {
                g_text_substitution_values[0] = unit->action_command.skillset_id | TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
            } else {
                g_text_substitution_values[0] = unit->action_command.ability_id + TEXT_ID_ABILITY_NAME_BASE;
            }
            message = 0x8001;
        } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
            & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
            message = 0x8002;
        }
    }
    /* Messages 0x10xx always show; 0x80xx are navigation messages that the option turns off.
     * The test compares the message's high byte with the option bits themselves, which works
     * because GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS is also 0x8000. */
    if (message != 0) {
        mode = g_main_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK;
        if (mode == GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS && (message & 0xff00) == mode) {
            sound_queue_sfx_confirm_unless_restoring();
        } else if (g_menu_restore_pending == 0) {
            sound_queue_sfx_invalid();
            g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
            thread_wait_until_inactive(g_thread_current_id - 1);
        }
        if (message == 0x1002 || message == 0x1003) {
            thread_exit_current();
        }
    } else {
        sound_queue_sfx_confirm_unless_restoring();
    }
}

/* Check a chosen action-menu row before it opens. With check_unit, deep water, Frog or the menu's
 * own restriction shows its message, plays the invalid sound and ends the thread; an Arithmeticks
 * row shows navigation message 0x8009 - value. A row with no message and a negative action mode is
 * confirmed at once without sound. */
void menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit) {
    s32 mode;
    s32 message;
    menu_entry_t* parameter;
    unit_stats_t* unit;

    g_menu_active_menu = menu;
    parameter = (menu_entry_t*)thread_get_current_parameter_1();
    mode = 0;
    message = 0;
    unit = menu_get_selected_unit_stats();
    if (check_unit != 0) {
        if (battle_unit_get_action_restriction(unit) == BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER) {
            message = 0x1003;
            mode = 2;
        } else {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
                message = 0x1007;
                mode = 2;
            } else {
                mode = 2;
                message = g_menu_restriction_message_ids[menu] + 0x1000;
            }
        }
    }
    if (message == 0 && menu == ACTION_MENU_TYPE_ARITHMETICKS) {
        message = 0x8009 - value;
        mode = 1;
    }
    if (mode != 0) {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_OFF || (message & 0xff00) != 0x8000) {
            g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
        }
        if (mode == 2) {
            sound_queue_sfx_invalid();
            if (g_main_game_options.fields.navigation_messages == GAME_OPTION_ON) {
                thread_wait_until_inactive(g_thread_current_id - 1);
            }
            thread_exit_current();
        }
    } else {
        mode = g_menu_action_type_modes
            [g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index]][0];
        if (mode < 0) {
            g_menu_action_slot_row_actions[0] = mode;
            menu_confirm_action_silently(parameter);
            thread_exit_current();
        }
    }
    g_menu_restore_depth = 2;
}

/* Thread body: validates the skill chosen in menu entry 3
 * for the current menu (MP cost, blocking status, per-menu restrictions) and, when it is not usable, shows the matching
 * message before ending the thread. */
void menu_validate_skill_selection_thread(void) {
    s32 skill = g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index;
    menu_entry_t* parameter = (menu_entry_t*)thread_get_current_parameter_1();
    s32 ct;
    s32 entry;
    s32 mp_cost;
    s32 message;
    s32 blocked;
    unit_stats_t* unit;
    menu_ability_list_t* skill_tables = &g_ai_workspace_ptr->ability_list;

    ct = skill_tables->skill_ct[skill];
    entry = g_menu_ability_layout.columns.text_colors[0][skill];
    mp_cost = skill_tables->mp_costs[skill];
    if (ct == 0xFF) {
        ct = 0;
    }
    message = 0;
    blocked = skill_tables->flags[skill] & 2;
    unit = menu_get_selected_unit_stats();
    if (g_menu_active_menu == ACTION_MENU_TYPE_DEFAULT && unit->mp - mp_cost < 0) {
        message = 0x800A;
        if (ct == 0) {
            message = 0x1008;
        }
    }
    g_menu_selected_skill_ct = ct;
    if ((g_menu_active_menu == ACTION_MENU_TYPE_MONSTER || g_menu_active_menu == ACTION_MENU_TYPE_DEFAULT)
        && blocked != 0
        && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)] & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
        message = 0x8013;
        if (ct == 0) {
            message = 0x1009;
        }
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        if (entry != 0) {
            message = 0x100B;
        }
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_WEAPON_INVENTORY && entry != 0) {
        message = 0x100C;
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        if (entry != 0) {
            message = 0x100D;
        }
    }
    g_menu_result = -4;
    if (message != 0) {
        if (g_main_game_options.fields.navigation_messages == GAME_OPTION_OFF) {
            sound_queue_sfx_confirm_unless_restoring();
        } else {
            g_menu_modal_open = 1;
            g_menu_selected_option = 0;
            g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
            sound_queue_sfx_invalid();
            thread_wait_until_inactive(g_thread_current_id - 1);
            g_menu_modal_open = 0;
        }
        if ((message != 0x800A && message != 0x8013) || g_menu_selected_option == -1) {
            g_menu_result = -1;
        }
    } else {
        sound_queue_sfx_confirm_unless_restoring();
    }
    menu_confirm_action_silently(parameter);
    thread_exit_current();
}

/* 1 when the skillset is in g_text_spell_quote_exception_skillsets (ended by SKILLSET_ID_NONE).
 * text_select_ability_announcement shows no spell quote and no ability name for such skillsets,
 * except Item and Throw, whose names still show. */
static s32 text_is_spell_quote_exception_skillset(s32 skillset_id) {
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

/*
 * Resolve the pending action-menu command, option, and item selection.
 *
 * Remaps menu-specific values through the current menu's command map, publishes the
 * resolved selection, and returns command | option << 8 | item << 16.
 */
s32 menu_resolve_selection(void) {
    s32 command = (u8)g_menu_pending_selection[0];
    s32 option = (u8)g_menu_pending_selection[1];
    u32 item = (u16)g_menu_pending_selection[2];
    s32 index;
    s32 i;
    s32 menu;

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
                        option = MENU_SELECTION_NONE;
                    } else if (menu == BATTLE_MENU_ID_MESSAGE_YES_NO || menu >= 0x64) {
                        command = *(g_menu_command_maps[i].map + option + 1);
                        option = MENU_SELECTION_NONE;
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
            option = MENU_SELECTION_NONE;
            item = MENU_SELECTION_NONE;
            g_menu_resolved_command = command;
        }
        g_menu_selected_command.kind = command;
        if ((option & 0xFE) != 0xFE) {
            option = g_menu_skillsets[index];
            if (option == SKILLSET_ID_ATTACK || option == SKILLSET_ID_DEFEND || option == SKILLSET_ID_EQUIP_CHANGE
                || option == SKILLSET_ID_BLANK_04 || option == SKILLSET_ID_ELEMENTAL || option == SKILLSET_ID_JUMP) {
                g_menu_selected_action.ability_id = 0;
                item = MENU_SELECTION_NONE;
            } else if (option == ACTION_MENU_PSEUDO_SKILLSET_ANYTHING && g_menu_skillset_build_result == 0) {
                item = g_menu_anything_ability_id;
            } else {
                item = g_ai_workspace_ptr->ability_list.ids[item] & 0x1FF;
            }
            g_menu_selected_action.skillset_id = option;
            g_menu_used_skillset_id = option;
            if (item != MENU_SELECTION_NONE) {
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
static void menu_run_system_function_thread(void) {
    s32 id;
    s32 menu_id;
    menu_system_function_t* entry;

    g_menu_system_function_thread_busy = 1;
    text_init_section_pointers();
    id = g_thread_contexts[g_thread_current_id].function_parameter_1;
    if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
        id = g_menu_system_function_table[id].alternate_id;
    }
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        while (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
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
static void menu_start_system_function_thread(s32 system_function) {
    menu_clear_entry_selections();
    thread_start(THREAD_SLOT_SYSTEM_FUNCTION, menu_run_system_function_thread);
    thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, system_function, 0, 0);
}

/* Confirm the current menu action, recording the auto-battle mode it selects. */
void menu_confirm_auto_battle_selection_thread(void) {
    unit_stats_t* attacker;
    s32 menu;
    menu_entry_t* parameter = (menu_entry_t*)thread_get_current_parameter_1();

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

/* Choose how an ability is announced: with the caster's spell quote, or with its name.
 *
 * A spell quote (SPELL_QUOTE) needs a skillset outside the exception list and not one the
 * unit's spell_quote_skillsets flags suppress, an ability other than the last one quoted
 * (g_text_spell_quote_last_ability_id) that is no item and has the spell-quote flag, a unit
 * that is not a Calculator or Mime and neither Transparent nor a Frog, ability names on, and a
 * 4 in 16 roll of the event RNG. Otherwise ABILITY_ANNOUNCEMENT runs: enabled when the
 * ability's type flags ask for its name (names on) or it is an Item or Throw, else with enabled
 * cleared. */
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
 * keep its record in g_menu_dead_unit_context and return the treasure item when
 * the result is 4, otherwise -1. */
s32 menu_get_dead_unit_selection(s32 battle_id) {
    unit_stats_t* stats = battle_unit_get_stats_from_battle_id(battle_id);
    crystal_pickup_result_t* result;

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

/* Dispatch a battle menu system command (battle_menu_system_command_e) for unit_id. The scenario
 * check may start an event (game over, a scenario condition's event, battle start, battle won);
 * with option 2 it only records the event to run next (g_script_next_event_id). Each other
 * command starts one system function thread, a message or menu, after setting its text values.
 * Returns a MENU_SYSTEM_DISPATCH_* result and remembers the command and option. */
static s32 menu_dispatch_system_function(
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
            if (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
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
                if ((battle_action_check_battle_outcome() < 0 || count != 0)
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
                } else if (battle_action_check_battle_outcome() == 0) {
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
                    if (g_menu_used_item_id == MENU_SELECTION_NONE || g_menu_selected_skill_ct == 0) {
                        value = 0x2D;
                    } else {
                        value = 0xC;
                    }
                } else {
                    if (g_menu_used_item_id != MENU_SELECTION_NONE || g_menu_selected_skill_ct == 0) {
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
        thread_start(THREAD_SLOT_SYSTEM_FUNCTION, text_run_spell_quote_thread);
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
        /* With 99 already held, the found item is sold for a quarter of its price instead. The
         * discarded data pointer lookups are retail. */
        if (main_item_get_total_equipment_quantity(option, 1) == 0x63) {
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

/* Resolves an event unit id to battle stats for the scenario conditions; returns
 * -1 for none.
 *
 * An id found among the ENTD unit ids gives that unit when it has a unit record.
 * Otherwise an id in the special-character identity range is looked up by
 * character identity and accepted only for a deployed party member (unit id
 * 0x78..0x7c). */
static unit_stats_t* unit_get_stats_by_unit_id(s32 unit_id) {
    s32 index;
    unit_stats_t* stats;

    battle_unit_find_stats_by_entd_unit_id(unit_id, &index);
    if (index < 0) {
        if (unit_id < CHARACTER_IDENTITY_RAMZA_CHAPTER_1 || unit_id >= CHARACTER_IDENTITY_GENERIC_FIRST) {
            return (unit_stats_t*)-1;
        }
        index = battle_unit_find_slot_by_character_identity(unit_id);
        if (index >= 0) {
            stats = battle_unit_get_stats_from_battle_id(index);
            if (stats->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && stats->unit_id < EVENT_UNIT_ID_DEPLOYED_END) {
                return stats;
            }
        }
        return (unit_stats_t*)-1;
    }
    if (battle_unit_get_runtime_id_by_battle_id(index) >= 0) {
        return battle_unit_get_stats_from_battle_id(index);
    }
    return (unit_stats_t*)-1;
}

/* Walk the scenario's condition chains and report whether one selects an event.
 *
 * g_main_scenario_condition_offsets holds up to ten byte offsets into
 * g_main_scenario_condition_data, one per chain; a zero offset after the first
 * ends the list. A chain is a run of records, each a 16-bit type followed by
 * g_script_condition_param_lengths[type] 16-bit operands. Records are
 * evaluated while they pass and the first failure drops the chain; the RUN_EVENT
 * record that closes a chain stores its event id in the current-event variable,
 * and the function returns 1. It returns 0 when no chain gets that far.
 *
 * The checker always receives the five 16-bit values at the record, whatever the
 * type's length. Called at EventEnd and by the scenario-check command of
 * the scene's menu system-function dispatcher. */
s32 script_run_scenario_conditions(void) {
    s16* condition;
    s32 i;
    s32 shifted_offset;
    s32 offset;
    s32 result;

    for (i = 0; i < 10; i++) {
        /* The entry is a 16-bit byte offset into the script area; the
         * target sign-extends it with a live `<< 16` value, taking the
         * division's sign bit from that value rather than from the
         * sign-extended one. Spelled as `offset = (s16)g_main_scenario_condition_offsets[i]` with
         * `offset / 2`, combine folds the shift pair into a single `lh` and the
         * function comes out three instructions short, so the shift pair
         * and the by-two division are spelled out here. */
        shifted_offset = g_main_scenario_condition_offsets[i] << 16;
        offset = shifted_offset >> 16;
        if (i != 0 && offset == 0) {
            return 0;
        }
        condition = &g_main_scenario_condition_data[(offset + (s32)((u32)shifted_offset >> 31)) >> 1];
        do {
            result
                = script_check_scenario_condition(condition[0], condition[1], condition[2], condition[3], condition[4]);
            condition += g_script_condition_param_lengths[condition[0]] + 1;
        } while (result == EVENT_SCENARIO_CONDITION_RESULT_PASS);
        if (result == EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT) {
            return 1;
        }
    }
    return 0;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_run_scenario_conditions and script_check_scenario_condition. Nothing
 * on the disc calls it or points at it; the name is a neutral placeholder
 * until a reference turns up. */
void script_noop_9(void) {
}

/* Evaluate one scenario condition record for
 * script_run_scenario_conditions.
 *
 * The record's 16-bit values arrive as type (event_scenario_condition_e), id and
 * value; the two tile conditions also read a y and an elevation, with value as
 * the x. Returns EVENT_SCENARIO_CONDITION_RESULT_PASS when the condition holds,
 * RESULT_RUN_EVENT for type RUN_EVENT after storing id as the current event, and
 * RESULT_FAIL otherwise, including for every type that has no test here.
 *
 * ACTING_UNIT looks at the unit the check runs for
 * (g_script_condition_unit_battle_id): it passes when that unit's unit id
 * is id, or when it is a deployed unit whose character identity is id and id is a
 * named (non-generic) identity. While the battle id is 0xff the condition fails.
 * ITEM_EQUIPPED searches the seven equipment slots of the units with unit ids 1
 * to 3. The HP percentage is hp * 100 / (max_hp + 1). The casualty conditions
 * compare with id, not value.
 *
 * Keep the two unit-id range comparisons: retail reloads the byte. */
static event_scenario_condition_result_e script_check_scenario_condition(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation) {
    unit_stats_t* unit;
    s32 i;
    s32 j; /* equipment slot, then battle index: one variable (a0 in both loops) */

    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_EQUAL && script_get_variable(id) == value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_LEAST && script_get_variable(id) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_MOST && value >= script_get_variable(id))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type >= EVENT_SCENARIO_CONDITION_UNIT_PRESENT && type <= EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST) {
        unit = unit_get_stats_by_unit_id(id);
        if (unit != (unit_stats_t*)-1) {
            if (type == EVENT_SCENARIO_CONDITION_UNIT_PRESENT)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_LEAST && unit->hp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_MOST && value >= unit->hp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_LEAST
                && unit->hp * 100 / (unit->max_hp + 1) >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_MOST
                && value >= unit->hp * 100 / (unit->max_hp + 1))
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_LEAST && unit->mp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST && value >= unit->mp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_ACTING_UNIT && g_script_condition_unit_battle_id != 0xff) {
        unit = battle_unit_get_stats_from_battle_id(g_script_condition_unit_battle_id);
        if (unit->unit_id == id
            || (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END
                && unit->character_identity == id && id >= 0 && id < CHARACTER_IDENTITY_GENERIC_FIRST
                && id != CHARACTER_IDENTITY_ENTD_NONE))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
    }
    if (type == EVENT_SCENARIO_CONDITION_ITEM_EQUIPPED) {
        for (i = 1; i < 4; i++) {
            unit = unit_get_stats_by_unit_id(i);
            if (unit != (unit_stats_t*)-1) {
                for (j = 0; j < 7; j++) {
                    if (unit->equipment[j] == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_MOST && value >= script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE) {
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) < id)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && value >= script_get_variable(EVENT_SCRIPT_VAR_DAY))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_AFTER) {
        if (id < script_get_variable(EVENT_SCRIPT_VAR_MONTH))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && script_get_variable(EVENT_SCRIPT_VAR_DAY) >= value)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) >= id)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_MOST && id >= script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_ENEMIES_DEFEATED && battle_action_check_battle_outcome() == 0)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_UNIT_AT_TILE) {
        unit = unit_get_stats_by_unit_id(id);
        if (unit != (unit_stats_t*)-1 && unit->x == value && unit->position.y == tile_y
            && unit->position.higher_elevation == elevation)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_PALETTE_UNIT_AT_TILE) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            if (battle_unit_has_runtime_id(i)) {
                j = battle_unit_get_battle_id_by_runtime_id(i);
                if (j != -1) {
                    unit = battle_unit_get_stats_from_battle_id(j);
                    if (unit->x == value && unit->position.y == tile_y && unit->position.higher_elevation == elevation
                        && unit->sprite_palette == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_RUN_EVENT) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, id);
        return EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT;
    }
    return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
}

/* Return the current event's 16-bit finish-operation value, or 0 while no scenario event
 * is active. Bits 0-7 and 10-11 hold the next scenario id and the remaining bits
 * the after-event code. */
s32 script_get_current_scenario_finish_operation(void) {
    if (g_script_scenario_event_active == 0) {
        return 0;
    }
    return g_main_scenario_event_finish_operations[script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
}

/* Return the after-event code of the pending event (g_script_next_event_id): bits
 * 8-9 and 12-15 of its 16-bit finish-operation value, shifted down by 8. Zero means the
 * event has none. */
u32 script_get_event_finish_operation(void) {
    return (g_main_scenario_event_finish_operations[g_script_next_event_id] & 0xF300) >> 8;
}

/* While a tutorial scripts the input, hold wait_value in the tutorial wait
 * variable for one thread yield with every controller input disabled. */
void script_pulse_tutorial_wait_value(s32 wait_value) {
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, wait_value);
        script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0xFFFF);
        thread_yield();
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, 0);
    }
}

/* Apply disabled and forced input variables while retaining the high input bits. */
static void script_apply_input_overrides(u32* state) {
    u32 high;

    if (*state == MAIN_INPUT_RESET_BUTTONS) {
        main_system_reset_game();
    }

    high = *state & 0xFFFF0000;
    *state = *state & (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) ^ 0xFFFF);
    *state = high | (script_get_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS) | *state);
}

#ifdef FFT_BATTLE
/* Route controller input to the running battle event thread. */
static void script_route_event_input(void) {
    s32 thread;

    if (g_menu_input_mode != MENU_INPUT_TUTORIAL) {
        return;
    }
    thread = g_thread_current_id;
    if (thread == THREAD_SLOT_MAIN) {
        return;
    }
    if (g_script_event_input == 0) {
        g_battle_script_saved_event_input = 0;
    }
    if (g_thread_contexts[thread].task_id == THREAD_TASK_RESUME) {
        g_script_event_input = g_script_unfiltered_controller_input;
    } else {
        g_script_event_input = g_battle_script_saved_event_input;
    }
}
#endif

#ifdef FFT_WORLD
/* WORLD twin of script_update_event_input_state: while the event thread
 * (0x10) is current, latch the pad state and derive the new-press mask with
 * the shared initial/repeat/secondary key-repeat delays. */
static void script_route_event_input(void) {
    u32 input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    if (g_thread_current_id != WORLD_THREAD_SLOT_TUTORIAL_TEXT) {
        return;
    }
    input = world_input_get_tutorial_buttons();
    if (input == 0) {
        g_world_event_initial_repeat_counter = 0;
        g_world_event_repeat_counter = 0;
        g_world_event_secondary_repeat_counter = 0;
    }

    previous_input = g_world_event_current_input;
    g_world_event_current_input = input;
    g_script_event_current_input = input;
    g_script_event_input = 0;
    g_world_event_previous_input = previous_input;

    if (previous_input != input || (input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_script_event_input = ~previous_input & input;
        g_world_event_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_world_event_initial_repeat_counter;
        if (counter == initial_delay) {
            g_script_event_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK);
        }
        if (counter >= initial_delay) {
            repeat_delay = g_main_input_repeat_period / g_script_event_speed;
            counter = ++g_world_event_repeat_counter;
            g_world_event_secondary_repeat_counter++;
            if (counter >= repeat_delay) {
                g_world_event_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_script_event_input = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_repeat_delay = g_main_input_secondary_repeat_period / g_script_event_speed;
            if (g_world_event_secondary_repeat_counter >= secondary_repeat_delay) {
                g_world_event_secondary_repeat_counter = 0;
                if ((g_world_event_current_input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input
                        |= g_world_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_script_event_input |= g_world_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_world_event_repeat_counter = 0;
            g_world_event_secondary_repeat_counter = 0;
        }
    }
}
#endif

/* Keep the raw 32-bit button value in g_script_unfiltered_controller_input, then
 * apply the script's disabled and forced inputs to it in place. */
void script_filter_controller_input(u32* input) {
    g_script_unfiltered_controller_input = *input;
    script_apply_input_overrides(input);
}

#ifdef FFT_BATTLE
/* Run one frame of BATTLE bookkeeping around the thread scheduler.
 *
 * BATTLE's body: returns the scenario-finish result
 * (9, 0xA, 0xB or 0x13, 0 when no finish operation is pending) once every
 * script thread has stopped, otherwise 1 (or g_script_frame_result_override while it is set, or 0
 * in tutorial input mode). The three BATTLE-only 16-bit clears come
 * after the 32-bit clears in source; the scheduler still groups them with the
 * other 16-bit stores, and this order keeps the g_menu_companion_overlay_state load
 * below them as in the target.
 */
s32 script_run_event_frame(u32* ot, u32 buttons) {
    RECT rect;
    s32 i;
    s32 finish_op;
    s32 next_event;
    u32 prims;
    u32* ordering_table;
    s32 polarity;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    /* While an END graphic is up (immediate drawing), reload its image from the high
     * overlay area into the frame buffer every frame. */
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawSync(0);
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0xF0;
        rect.y = (g_main_gfx_screen_polarity & 1) * 0xF0;
        LoadImage(&rect, (u32*)g_main_heap_high_overlay_load_address);
    }
    g_gfx_current_otag_entry = ot;
    gfx_submit_screen_fade_overlay(&g_battle_gfx_screen_fade_overlays[g_menu_packet_buffer_index], g_gfx_screen_fade);
    camera_store_state_to_script_variables();
    menu_draw_active_window_frames();

    if (g_script_threads_idle != 0) {
        for (i = 1; i < THREAD_ID_COUNT; i++) {
            if (thread_is_running_asm(i) != 0) {
                break;
            }
        }
        if (i == THREAD_ID_COUNT) {
            finish_op = script_get_current_scenario_finish_operation();
            if (finish_op != 0) {
                if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_script_threads_idle = 0;
                    script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return 9;
                }
            } else {
                g_script_threads_idle = 0;
                g_menu_overlay_state = 0;
                return 0;
            }
            if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_FUNERAL) {
                return 0x13;
            }
            /* The finish word holds the next event id in bits 0-7 and 10-11 (its bits 8-9);
             * the remaining high bits give the finish result. */
            next_event = ((finish_op & 0xC00) >> 2) | (finish_op & 0xFF);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, next_event);
            finish_op = (finish_op & 0xF300) >> 8;
            g_script_threads_idle = 0;
            g_menu_overlay_state = 0;
            if (finish_op == 0x80) {
                return 9;
            }
            if (finish_op == 0x81) {
                return 0xA;
            }
            if (finish_op == 0x82) {
                return 0xB;
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_main_entd_selection_mode = BATTLE_ENTD_SELECTION_MAP_ID;
            return 9;
        }
        g_script_threads_idle = 0;
    }

    script_update_event_input_state(buttons);
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        g_script_unfiltered_controller_input = g_script_event_input;
        script_apply_input_overrides(&g_script_event_input);
        g_battle_script_saved_event_input = g_script_event_input;
        script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
    }
    if (g_battle_map_reload_state >= 3) {
        g_battle_map_reload_state = 0;
    }
    if (g_battle_map_append_state >= 3) {
        g_battle_map_append_state = 0;
        g_map_destruction_wait_state = 1;
    }

    /* Clear last frame's requests; script commands raise them again while the threads run. */
    g_event_pending_map_jump_out_id = -1;
    g_event_weather_request = -1;
    g_event_pending_map_state = -1;
    g_event_pending_map_jump_out_2_id = -1;
    g_event_pending_effect_id = -1;
    g_script_pending_loaded_evtchr_slot = -1;
    g_event_pending_evtchr_save_slot = -1;
    g_event_pending_saved_evtchr_clear_slot = -1;
    g_event_pending_loaded_evtchr_clear_slot = -1;
    g_event_pending_unit_vram_copy = -1;
    g_event_pending_reserved_vram_release_slot = -1;
    g_sound_pending_sfx_id = -1;
    g_battle_sound_music_volume_transition_request = -1;
    g_battle_sound_music_set_track_request = -1;
    g_battle_sound_music_switch_request = -1;
    g_battle_sound_music_unload_slot_request = -1;
    g_event_map_jump_in_request = 0;
    g_battle_3d_object_use_request = 0;
    g_battle_field_object_use_request = 0;

    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_JOBSTTS) {
        ordering_table = jobstts_entrypoint(g_menu_selected_unit.battle_id, 0);
        g_menu_companion_overlay_primitives = (s32)ordering_table;
        if (ordering_table != 0) {
            AddPrims(g_gfx_current_otag_entry, ordering_table, &ordering_table[0x3f]);
        } else {
            g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }
    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_EQUIP) {
        prims = equip_entrypoint(g_menu_selected_unit.battle_id, g_gfx_current_otag_entry + 1);
        g_menu_companion_overlay_primitives = prims;
        if (prims >= 2) {
            AddPrims(g_gfx_current_otag_entry, (void*)prims, (void*)(prims + 0xFC));
        } else {
            g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }

    /* A press other than Select alone, or a thread starting or stopping while the threads
     * run (thread_yield), restarts the five-frame input suppression. */
    if (script_is_tutorial_event_slot() == 0 && g_script_event_input != PSX_PAD_SELECT && g_script_event_input != 0) {
        g_script_input_suppression_frames = 5;
    }
    for (i = 1; i < THREAD_ID_COUNT; i++) {
        g_event_thread_status_snapshot[i] = thread_is_running_asm(i);
    }
    thread_yield();
    for (i = 1; i < THREAD_ID_COUNT; i++) {
        if (g_event_thread_status_snapshot[i] != thread_is_running_asm(i)) {
            g_script_input_suppression_frames = 5;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }

    camera_load_state_from_script_variables();
    g_battle_event_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
    script_frame_after_threads_stub();
    if (g_script_frame_result_override != 0) {
        return g_script_frame_result_override;
    }

    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_STUB) {
        menu_run_companion_overlay_stub();
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_BUNIT) {
        script_set_event_speed(1);
        i = bunit_entrypoint(0, 0);
        battle_target_move_cursor_to_battle_id(i);
        g_menu_selected_unit.battle_id = i;
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_CARD_SAVE) {
        script_set_event_speed(1);
        /* The save menu runs frames of its own and changes the screen polarity; keep the event's. */
        polarity = g_main_gfx_screen_polarity;
        script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_save_run_menu(0);
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        g_main_gfx_screen_polarity = polarity;
    }

    menu_open_companion_executable();
    menu_open_companion_data_file();
    script_process_pending_requests();
    script_frame_after_requests_stub();
    if (g_menu_input_mode == MENU_INPUT_BLOCKED) {
        return 1;
    }
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        return 0;
    }

    /* Once every thread has stopped, the next frame reports the scenario finish. */
    for (i = 1; i < THREAD_ID_COUNT; i++) {
        if (thread_is_running_asm(i) != 0) {
            break;
        }
    }
    if (i == THREAD_ID_COUNT) {
        g_script_threads_idle = 1;
    }
    return 1;
}
#endif

#ifdef FFT_WORLD
/* Run one frame of WORLD bookkeeping around the thread scheduler.
 *
 * Returns the scenario-finish result (9, 0xA, 0xB or 0x13, 0 when no finish
 * operation is pending) once every script thread has stopped, otherwise 1
 * (or g_script_frame_result_override while it is set, or 0 in tutorial input mode).
 * Unlike BATTLE's, it also checks thread slot 16 for idleness, and before running the threads
 * it passes the event input globals themselves to script_load_event_input_state.
 */
s32 script_run_event_frame(u32* ot, u32 buttons) {
    RECT rect;
    s32 i;
    s32 finish_op;
    s32 next_event;
    u32 prims;
    u32* ordering_table;
    s32 polarity;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    /* While an END graphic is up (immediate drawing), reload its image from the high
     * overlay area into the frame buffer every frame. */
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawSync(0);
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0xF0;
        rect.y = (g_main_gfx_screen_polarity & 1) * 0xF0;
        LoadImage(&rect, (u32*)g_main_heap_high_overlay_load_address);
    }
    g_gfx_current_otag_entry = ot;
    gfx_submit_screen_fade_overlay(&g_world_screen_tint_tiles[g_menu_packet_buffer_index], g_gfx_screen_fade);
    camera_store_state_to_script_variables();
    menu_draw_active_window_frames();

    if (g_script_threads_idle != 0) {
        for (i = 1; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (thread_is_running_asm(i) != 0) {
                break;
            }
        }
        if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
            finish_op = script_get_current_scenario_finish_operation();
            if (finish_op != 0) {
                if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_script_threads_idle = 0;
                    script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return 9;
                }
            } else {
                g_script_threads_idle = 0;
                g_menu_overlay_state = 0;
                return 0;
            }
            if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_FUNERAL) {
                return 0x13;
            }
            /* The finish word holds the next event id in bits 0-7 and 10-11 (its bits 8-9);
             * the remaining high bits give the finish result. */
            next_event = ((finish_op & 0xC00) >> 2) | (finish_op & 0xFF);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, next_event);
            finish_op = (finish_op & 0xF300) >> 8;
            g_script_threads_idle = 0;
            g_menu_overlay_state = 0;
            if (finish_op == 0x80) {
                return 9;
            }
            if (finish_op == 0x81) {
                return 0xA;
            }
            if (finish_op == 0x82) {
                return 0xB;
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_main_entd_selection_mode = BATTLE_ENTD_SELECTION_MAP_ID;
            return 9;
        }
        g_script_threads_idle = 0;
    }

    script_update_event_input_state(buttons);
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        g_script_unfiltered_controller_input = g_script_event_input;
        script_apply_input_overrides(&g_script_event_input);
        g_world_script_saved_event_input = g_script_event_input;
        script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
    }
    if (g_world_map_reload_state >= 3) {
        g_world_map_reload_state = 0;
    }
    if (g_world_map_append_state >= 3) {
        g_world_map_append_state = 0;
        g_map_destruction_wait_state = 1;
    }

    /* Clear last frame's requests; script commands raise them again while the threads run. */
    g_event_pending_map_jump_out_id = -1;
    g_event_weather_request = -1;
    g_event_pending_map_state = -1;
    g_event_pending_map_jump_out_2_id = -1;
    g_event_pending_effect_id = -1;
    g_script_pending_loaded_evtchr_slot = -1;
    g_event_pending_evtchr_save_slot = -1;
    g_event_pending_saved_evtchr_clear_slot = -1;
    g_event_pending_loaded_evtchr_clear_slot = -1;
    g_event_pending_unit_vram_copy = -1;
    g_event_pending_reserved_vram_release_slot = -1;
    g_event_map_jump_in_request = 0;
    g_world_3d_object_use_request = 0;
    g_world_field_object_use_request = 0;

    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_JOBSTTS) {
        ordering_table = jobstts_entrypoint(g_menu_selected_unit.battle_id, 0);
        g_menu_companion_overlay_primitives = (s32)ordering_table;
        if (ordering_table != 0) {
            AddPrims(g_gfx_current_otag_entry, ordering_table, &ordering_table[0x3f]);
        } else {
            g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }
    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_EQUIP) {
        prims = equip_entrypoint(g_menu_selected_unit.battle_id, g_gfx_current_otag_entry + 1);
        g_menu_companion_overlay_primitives = prims;
        if (prims >= 2) {
            AddPrims(g_gfx_current_otag_entry, (void*)prims, (void*)(prims + 0xFC));
        } else {
            g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }

    script_load_event_input_state((menu_input_state_t*)&g_script_event_input);
    /* A press other than Select alone, or a thread starting or stopping while the threads
     * run (thread_yield), restarts the five-frame input suppression. */
    if (script_is_tutorial_event_slot() == 0 && g_script_event_input != PSX_PAD_SELECT && g_script_event_input != 0) {
        g_script_input_suppression_frames = 5;
    }
    for (i = 1; i < THREAD_ID_COUNT; i++) {
        g_event_thread_status_snapshot[i] = thread_is_running_asm(i);
    }
    thread_yield();
    for (i = 1; i < THREAD_ID_COUNT; i++) {
        if (g_event_thread_status_snapshot[i] != thread_is_running_asm(i)) {
            g_script_input_suppression_frames = 5;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }

    camera_load_state_from_script_variables();
    g_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
    script_frame_after_threads_stub();
    if (g_script_frame_result_override != 0) {
        return g_script_frame_result_override;
    }

    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_STUB) {
        menu_run_companion_overlay_stub();
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_BUNIT) {
        script_set_event_speed(1);
        i = bunit_entrypoint(0, 0);
        battle_target_move_cursor_to_battle_id(i);
        g_menu_selected_unit.battle_id = i;
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_CARD_SAVE) {
        script_set_event_speed(1);
        /* The save menu runs frames of its own and changes the screen polarity; keep the event's. */
        polarity = g_main_gfx_screen_polarity;
        script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_save_run_menu(0);
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        g_main_gfx_screen_polarity = polarity;
    }

    menu_open_companion_executable();
    menu_open_companion_data_file();
    script_process_pending_requests();
    script_frame_after_requests_stub();
    if (g_menu_input_mode == MENU_INPUT_BLOCKED) {
        return 1;
    }
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        return 0;
    }

    /* Once every thread has stopped, the next frame reports the scenario finish. */
    for (i = 1; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
        if (thread_is_running_asm(i) != 0) {
            break;
        }
    }
    if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
        g_script_threads_idle = 1;
    }
    return 1;
}
#endif

/* Feed `input` to the event input state, make `entry` the current ordering-table entry
 * and draw the active menu windows. No retail code calls it. */
void script_set_current_otag_entry_and_update_input(u32* entry, s32 input) {
    script_update_event_input_state(input);
    g_gfx_current_otag_entry = entry;
    menu_draw_active_window_frames();
}

/* Advance the map-destruction wait state from 1 (armed when a map append
 * finishes) to 2. script_process_pending_requests then releases the held
 * GNS data and polls the map load, returning the state to 0 when the load ends.
 * Any other state is left alone. */
void script_set_specialized_map_destroyed(void) {

    if (g_map_destruction_wait_state == 1) {
        g_map_destruction_wait_state = g_map_destruction_wait_state + 1;
    }
}

/* Advances asynchronous battle-script, map, effect, and graphics requests.
 * WORLD has none of them: its copy is empty. */
static void script_process_pending_requests(void) {
#ifdef FFT_BATTLE
    s16 destruction_state;
    s16 effect_id;
    s16 jump_out_map_id;
    s16 jump_out_2_map_id;
    s16 jump_in_duration;
    s16 time_of_day;
    s16 weather_request;
    s32 weather_id;
    s32 loaded_slot;
    s32 save_slot;
    s32 saved_clear_slot;
    s32 loaded_clear_slot;
    s32 release_slot;
    s32 sfx_id;
    s32 unit_vram_copy;
    u16 reload_state;
    s32 append_state;
    u16 music_track_id;
    u32 volume_transition;
    s32 pending = 0;
    effect_start_target_data_t effect_target;

    unit_update_facing_and_pending_animations();

    jump_out_map_id = g_event_pending_map_jump_out_id;
    if (jump_out_map_id != -1) {
        battle_state_start_map_jump_out(jump_out_map_id, 0x10);
    }
    jump_out_2_map_id = g_event_pending_map_jump_out_2_id;
    if (jump_out_2_map_id != -1) {
        battle_state_start_change_map_jump_out(jump_out_2_map_id, 0x10);
    }
    jump_in_duration = g_event_map_jump_in_request;
    if (jump_in_duration != 0) {
        battle_state_start_change_map_jump_in(jump_in_duration);
    }

    if (g_battle_3d_object_use_request != 0) {
        battle_map_command_set_3d_object_state(g_battle_event_3d_object_id, g_battle_event_3d_object_state);
        g_3d_object_wait_status = 1;
    }
    if (g_battle_field_object_use_request != 0) {
        battle_map_command_start_texture_animation(
            g_battle_event_map_command_83_arg1, g_battle_event_map_command_83_arg2);
        g_field_object_wait_status = 1;
    }
    if (g_3d_object_wait_status != 0) {
        g_3d_object_wait_status = battle_map_command_get_3d_object_state(g_battle_event_3d_object_id);
    }
    if (g_field_object_wait_status != 0) {
        g_field_object_wait_status
            = battle_map_command_get_texture_animation_active(g_battle_event_map_command_83_arg1);
    }

    if (g_map_refresh_pending != 0) {
        g_map_refresh_pending = battle_map_load_alternate_mesh(3);
    }
    /* Map reload and append step through states: 1 starts the load, 2 polls it, and 3 (done)
     * is cleared at the start of the next frame. */
    reload_state = g_battle_map_reload_state;
    if (reload_state == 1) {
        if (battle_map_start_data_load() == 0) {
            g_battle_map_reload_state = g_battle_map_reload_state + 1;
        }
    } else if (reload_state == 2) {
        if (battle_map_poll_data_load() == 0) {
            g_battle_map_reload_state = g_battle_map_reload_state + 1;
        }
    }
    append_state = g_battle_map_append_state;
    if ((append_state == 1) && (g_map_destruction_wait_state == 0)) {
        battle_map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), MAP_LOAD_COMMAND_START_HELD);
        g_battle_map_append_state = g_battle_map_append_state + 1;
    } else if ((append_state == 2)
        && (battle_map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), MAP_LOAD_COMMAND_POLL_HELD)
            == MAP_LOAD_RESULT_HELD)) {
        g_battle_map_append_state = g_battle_map_append_state + 1;
    }
    /* State 2 (set once a script or effect reports the map destroyed) releases the held map data;
     * 3 polls the load until it ends. */
    destruction_state = g_map_destruction_wait_state;
    if (destruction_state == 2) {
        g_map_destruction_wait_state = destruction_state + 1;
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_RELEASE_GNS_HOLD, 1, 1, 1);
    } else if ((destruction_state == 3)
        && (battle_map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), MAP_LOAD_COMMAND_POLL) == 0)) {
        g_map_destruction_wait_state = 0;
    }

    /* The low nibble picks the weather; without 0xF00 bits the request only clears. Snowy
     * maps use the snow variants five entries later. */
    weather_request = g_event_weather_request;
    if (weather_request != -1) {
        weather_id = weather_request & 0xF;
        if (!(weather_request & 0xF00)) {
            g_event_weather_request = -1;
        } else {
            g_event_weather_request = weather_id;
            if (g_battle_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                g_event_weather_request = weather_id + 5;
            }
            battle_map_run_weather_command(g_battle_event_weather_commands[g_event_weather_request].environment_id);
        }
    }

    time_of_day = g_event_pending_map_state;
    if (time_of_day != -1) {
        script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, time_of_day);
        battle_map_run_weather_command(MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
    }

    effect_id = g_event_pending_effect_id;
    if (effect_id != -1) {
        if (g_event_effect_target_mode == EFFECT_START_ON_CASTER) {
            effect_target.runtime_id = g_battle_event_effect_target_runtime_id;
            battle_effect_start_new(effect_id, EFFECT_START_ON_CASTER, effect_target);
        } else {
            effect_target.runtime_id = g_battle_event_effect_target_runtime_id;
            battle_effect_start_new(effect_id, EFFECT_START_ON_UNIT, effect_target);
        }
    }

    if (g_unit_graphics_load_pending != 0) {
        g_unit_graphics_load_pending = battle_gfx_poll_unit_graphics_load();
    }

    /* `pending` is never set, so the slot and sound requests always run. */
    if (pending == 0) {
        loaded_slot = g_script_pending_loaded_evtchr_slot;
        if (loaded_slot >= 0) {
            battle_gfx_unpack_evtchr_file_to_vram_slot(loaded_slot, g_script_loaded_evtchr_buffer);
        }
        save_slot = g_event_pending_evtchr_save_slot;
        if (save_slot >= 0) {
            battle_gfx_save_evtchr_slot(save_slot);
        }
        saved_clear_slot = g_event_pending_saved_evtchr_clear_slot;
        if (saved_clear_slot >= 0) {
            battle_gfx_clear_saved_evtchr_slot(saved_clear_slot);
        }
        loaded_clear_slot = g_event_pending_loaded_evtchr_clear_slot;
        if (loaded_clear_slot >= 0) {
            battle_gfx_clear_loaded_evtchr_slot(loaded_clear_slot);
        }
        /* High byte: the unit's runtime id; low byte: the event-character slot. */
        unit_vram_copy = g_event_pending_unit_vram_copy;
        if (unit_vram_copy >= 0) {
            battle_gfx_load_unit_attack_image_into_evtchr_slot((unit_vram_copy & 0xFF00) >> 8, unit_vram_copy & 0xFF);
        }
        release_slot = g_event_pending_reserved_vram_release_slot;
        if (release_slot >= 0) {
            battle_gfx_release_reserved_vram_slot(release_slot);
        }

        sfx_id = g_sound_pending_sfx_id;
        if (sfx_id != -1) {
            main_sound_play_sfx(sfx_id);
        }
        /* Alternate between the two track slots; nothing new starts when that slot is empty. */
        if (g_battle_sound_music_switch_request >= 0) {
            main_sound_stop_current_music();
            g_battle_sound_music_track_index = g_battle_sound_music_track_index ^ 1;
            if (g_battle_sound_music_track_index == 0) {
                music_track_id = g_sound_music_track_1_id;
            } else {
                music_track_id = g_sound_music_track_2_id;
            }
            if (music_track_id != 0) {
                main_sound_switch_music_track(g_battle_sound_music_track_index + 1,
                    script_scale_music_volume(g_battle_event_music_switch_volume),
                    g_battle_event_music_switch_fade * 4);
            }
        }
        if (g_battle_sound_music_unload_slot_request >= 0) {
            main_sound_stop_current_music();
            main_sound_unload_scenario_mus(g_battle_sound_music_unload_slot_request);
        }
        if (g_battle_sound_music_set_track_request != -1) {
            main_sound_unload_current_scenario_music();
            main_sound_set_current_music_track(g_battle_sound_music_set_track_request);
        }
        /* Low half: target volume; high half: fade time, passed times four like the switch fade. */
        volume_transition = g_battle_sound_music_volume_transition_request;
        if (volume_transition != -1U) {
            main_sound_set_current_music_target(volume_transition & 0xFFFF, (volume_transition >> 0xE) & 0x3FFC);
        }
    }
#endif
}

/* Load camera rotation, zoom and position back out of the script variable
 * block; the inverse of camera_store_state_to_script_variables. WORLD has
 * no camera, so its copy is empty. */
static void camera_load_state_from_script_variables(void) {
#ifdef FFT_BATTLE
    setVector(&g_battle_camera_script_rotation, g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT],
        g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW], g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ROLL]);
    battle_camera_set_rotation(&g_battle_camera_script_rotation);

    setVector(&g_battle_camera_script_vector, g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM],
        g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM], g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM]);
    battle_camera_set_zoom(&g_battle_camera_script_vector);

    g_battle_camera_script_vector.vx = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_X];
    g_battle_camera_script_vector.vy = (g_camera_shake_z_offset * ONE) + g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Z];
    g_battle_camera_script_vector.vz = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Y];
    battle_camera_set_current_real_coords(&g_battle_camera_script_vector);
#endif
}

/* Publish the live camera rotation, zoom, and position to the script variable
 * block. WORLD has no camera, so its copy is empty.
 *
 * The three zoom components all land in the same 32-bit script variable; that is what the
 * target does. */
void camera_store_state_to_script_variables(void) {
#ifdef FFT_BATTLE
    SVECTOR* rotation;
    VECTOR* zoom;
    VECTOR* coords;

    rotation = battle_camera_get_rotation();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT] = rotation->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] = rotation->vy;
    g_battle_camera_published_rotation_ptr = rotation;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ROLL] = rotation->vz;

    zoom = battle_camera_get_zoom();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vy;
    g_battle_camera_published_coords_ptr = zoom;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vz;

    coords = battle_camera_get_current_real_coords();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_X] = coords->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Z] = coords->vy - (g_camera_shake_z_offset * ONE);
    g_battle_camera_published_coords_ptr = coords;
    g_camera_shake_z_offset = 0;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Y] = coords->vz;
#endif
}

/* The raw weather script variable. battle_map_get_effective_weather applies the
 * map's indoor and snow flags to it. */
s32 map_get_weather(void) {
    return g_script_variables[EVENT_SCRIPT_VAR_WEATHER];
}
