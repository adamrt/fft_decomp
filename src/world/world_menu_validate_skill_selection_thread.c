#include "fft/battle.h"
#include "fft/world.h"

/* Thread body: validates the skill chosen in menu entry 3 for the current
 * menu (MP cost, blocking status, per-menu restrictions) and, when it is
 * not usable, shows the matching message before ending the thread. */
void world_menu_validate_skill_selection_thread(void) {
    s32 skill;
    void* parameter;
    s32 target;
    s32 entry;
    s32 mp_cost;
    s32 message;
    s32 blocked;
    battle_stats_t* unit;
    world_ability_skill_use_tables_t* skill_tables;

    skill = g_world_menu_thread_menu_data[3].selected_index;
    parameter = world_thread_get_current_parameter_1();
    skill_tables = (world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr;
    target = skill_tables->target[skill];
    entry = g_world_ability_menu_layout.row_actions[skill];
    mp_cost = skill_tables->mp_cost[skill];
    if (target == 0xFF) {
        target = 0;
    }
    message = 0;
    blocked = skill_tables->flags[skill] & 2;
    unit = world_unit_get_battle_stats_for_stored();
    if (g_world_active_menu == 0 && unit->mp - mp_cost < 0) {
        message = 0x800A;
        if (target == 0) {
            message = 0x1008;
        }
    }
    g_world_ability_selected_skill_target = target;
    if ((g_world_active_menu == 6 || g_world_active_menu == 0) && blocked != 0
        && (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE))) {
        message = 0x8013;
        if (target == 0) {
            message = 0x1009;
        }
    }
    if (g_world_active_menu == 1) {
        if (entry != 0) {
            message = 0x100B;
        }
    }
    if (g_world_active_menu == 2 && entry != 0) {
        message = 0x100C;
    }
    if (g_world_active_menu == 7) {
        if (entry != 0) {
            message = 0x100D;
        }
    }
    g_world_menu_result = -4;
    if (message != 0) {
        if (g_main_game_options.fields.navigation_messages == GAME_OPTION_OFF) {
            world_sound_set_effect_to_confirm_checked();
        } else {
            g_world_menu_transition_active = 1;
            g_world_menu_selected_option = 0;
            g_world_menu_thread_menu_data[35].text_id = message;
            world_thread_start(g_world_thread_current_id - 1, world_menu_icon_linked_entry_thread);
            world_thread_set_parameters(g_world_thread_current_id - 1, (s32)&g_world_menu_thread_menu_data[35], 0, 0);
            world_sound_set_effect_to_invalid();
            world_thread_wait_until_inactive(g_world_thread_current_id - 1);
            g_world_menu_transition_active = 0;
        }
        if ((message != 0x800A && message != 0x8013) || g_world_menu_selected_option == -1) {
            g_world_menu_result = -1;
        }
    } else {
        world_sound_set_effect_to_confirm_checked();
    }
    world_menu_confirm_action_silently((s32)parameter);
    world_thread_exit_current();
}
