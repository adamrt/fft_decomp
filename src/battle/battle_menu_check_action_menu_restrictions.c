#include "fft/battle.h"
#include "psx/types.h"

/*
 * Battle twin of world_menu_check_action_restrictions: check action-menu restrictions and show the applicable warning.
 *
 * Menu groups 0x13/0x14 and 0x30/0x31 use the battle eligibility check.
 * Other menus inspect the unit's current status and last action. The packed
 * options can suppress status warnings, but messages 0x1002/0x1003 always
 * stop the current thread after any warning thread finishes.
 * WORLD twin: world_menu_check_action_restrictions.
 */
void battle_menu_check_action_menu_restrictions(void) {
    s32 message = 0;
    battle_stats_t* unit;
    s32 menu;
    s32 result;
    s32 mode;

    unit = battle_unit_get_attacker_data_pointer();
    menu = g_battle_menu_current_id;
    if ((u32)(menu - 0x13) < 2 || (u32)(menu - 0x30) < 2) {
        result = battle_formula_can_unit_evade(unit);
        if (result == 0) {
            battle_sound_set_effect_to_invalid();
            battle_thread_exit_current();
        }
        if (result == 1) {
            message = 0x1002;
        }
        if (result == 2) {
            message = 0x1003;
        }
    } else {
        if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING)) {
            if (g_main_action_menu_types_by_skillset[unit->last_skillset_id] == ACTION_MENU_TYPE_CHARGE) {
                g_battle_text_substitution_values[0] = unit->last_skillset_id | 0xb000;
            } else {
                g_battle_text_substitution_values[0] = unit->last_ability_id + TEXT_ID_ABILITY_NAME_BASE;
            }
            message = 0x8001;
        } else if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING)) {
            message = 0x8002;
        }
    }
    if (message != 0) {
        mode = g_main_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK;
        if (mode == GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS && (message & 0xff00) == mode) {
            battle_sound_set_effect_to_confirm_checked();
        } else if (g_battle_menu_restore_pending == 0) {
            battle_sound_set_effect_to_invalid();
            g_battle_menu_thread_menu_data[35].text_id = message;
            battle_thread_start(g_battle_current_thread_id - 1, battle_menu_icon_linked_entry_thread);
            battle_thread_set_parameters(
                g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[35], 0, 0);
            battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
        }
        if ((u32)(message - 0x1002) < 2) {
            battle_thread_exit_current();
        }
    } else {
        battle_sound_set_effect_to_confirm_checked();
    }
}
