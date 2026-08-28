#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Battle twin of world_menu_validate_skill_selection_thread. Thread body: validates the skill chosen in menu entry 3
 * for the current menu (MP cost, blocking status, per-menu restrictions) and, when it is not usable, shows the matching
 * message before ending the thread. */
void battle_menu_validate_skill_selection_thread(void) {
    s32 skill;
    void* parameter;
    s32 target;
    s32 entry;
    s32 mp_cost;
    s32 message;
    s32 blocked;
    battle_stats_t* unit;

    skill = g_battle_menu_thread_menu_data[3].selected_index;
    parameter = (void*)battle_thread_get_current_parameter_1();
    target = ((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->target[skill];
    entry = g_battle_menu_ability_display_flags_ptr[skill];
    mp_cost = ((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->mp_cost[skill];
    if (target == 0xFF) {
        target = 0;
    }
    message = 0;
    blocked = ((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->flags[skill] & 2;
    unit = battle_unit_get_attacker_data_pointer();
    if (g_battle_active_menu == 0 && unit->mp - mp_cost < 0) {
        message = 0x800A;
        if (target == 0) {
            message = 0x1008;
        }
    }
    g_battle_menu_selected_skill_target = target;
    if ((g_battle_active_menu == 6 || g_battle_active_menu == 0) && blocked != 0
        && (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE))) {
        message = 0x8013;
        if (target == 0) {
            message = 0x1009;
        }
    }
    if (g_battle_active_menu == 1) {
        if (entry != 0) {
            message = 0x100B;
        }
    }
    if (g_battle_active_menu == 2 && entry != 0) {
        message = 0x100C;
    }
    if (g_battle_active_menu == 7) {
        if (entry != 0) {
            message = 0x100D;
        }
    }
    g_battle_menu_result = -4;
    if (message != 0) {
        if (g_main_game_options.fields.navigation_messages == GAME_OPTION_OFF) {
            battle_sound_set_effect_to_confirm_checked();
        } else {
            g_option_menu_open = 1;
            g_battle_menu_selected_option = 0;
            g_battle_menu_thread_menu_data[35].text_id = message;
            battle_thread_start(g_battle_current_thread_id - 1, battle_menu_icon_linked_entry_thread);
            battle_thread_set_parameters(
                g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[35], 0, 0);
            battle_sound_set_effect_to_invalid();
            battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
            g_option_menu_open = 0;
        }
        if ((message != 0x800A && message != 0x8013) || g_battle_menu_selected_option == -1) {
            g_battle_menu_result = -1;
        }
    } else {
        battle_sound_set_effect_to_confirm_checked();
    }
    battle_menu_confirm_action_silently(parameter);
    battle_thread_exit_current();
}
