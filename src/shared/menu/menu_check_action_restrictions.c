#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/types.h"

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
