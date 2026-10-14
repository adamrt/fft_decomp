#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/thread.h"

void menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit) {
    s32 mode;
    s32 message;
    menu_entry_t* parameter;
    unit_stats_t* unit;

    g_menu_active_menu = menu;
    parameter = (menu_entry_t*)battle_thread_get_current_parameter_1();
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
                message = g_battle_menu_restriction_message_ids[menu] + 0x1000;
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
            g_battle_menu_action_slot_row_actions[0] = mode;
            menu_confirm_action_silently(parameter);
            thread_exit_current();
        }
    }
    g_battle_menu_restore_depth = 2;
}
