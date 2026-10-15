/* Silent confirm, wait/move confirm threads and action restrictions. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Queue the confirm sound unless a menu restore is pending
 * (g_menu_restore_pending). */
void sound_queue_sfx_confirm_unless_restoring(void) {
    if (g_menu_restore_pending == 0) {
        g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    }
}

/* Runs the menu's action handler on a simulated, silent Circle press with
 * option 0 selected, then clears the injected input. */
void menu_confirm_action_silently(menu_entry_t* menu) {
    g_sound_suppressed = 1;
    g_script_event_input = PSX_PAD_CIRCLE;
    menu_handle_action(menu, 0);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
}

/* Menu-opening thread 6: the Wait menu. */
void menu_wait_thread(void) {
    menu_entry_t* menu;

    menu = (menu_entry_t*)thread_get_current_parameter_1_variant();
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
    menu_entry_t* entry;
    s32 menu;

    entry = (menu_entry_t*)thread_get_current_parameter_1_variant();
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

void menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit) {
    s32 mode;
    s32 message;
    menu_entry_t* parameter;
    unit_stats_t* unit;

    g_menu_active_menu = menu;
    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
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

/* Battle twin of menu_validate_skill_selection_thread. Thread body: validates the skill chosen in menu entry 3
 * for the current menu (MP cost, blocking status, per-menu restrictions) and, when it is not usable, shows the matching
 * message before ending the thread. */
void menu_validate_skill_selection_thread(void) {
    s32 skill;
    menu_entry_t* parameter;
    s32 ct;
    s32 entry;
    s32 mp_cost;
    s32 message;
    s32 blocked;
    unit_stats_t* unit;
    menu_ability_list_t* skill_tables;

    skill = g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index;
    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
    skill_tables = &g_ai_workspace_ptr->ability_list;
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
