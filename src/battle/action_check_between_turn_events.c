#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"

/* Runs any in-between-turn event, then opens the next acting unit's turn. */
void action_check_between_turn_events(void) {
    battle_unit_t* unit;

    if (map_try_start_deep_dungeon_mesh_load() != 0) {
        return;
    }
    action_clear_at_list_id();
    g_main_system_animation_speed = 1;
    action_run_between_turn_events();
    unit = battle_unit_get_source();
    if (unit == 0) {
        return;
    }
    g_battle_casting_runtime_id = unit->runtime_id;
    battle_unit_store_entd_flags_without_control_flag(unit);
    if (unit->stats != 0) {
        menu_reset_unit_record(unit->stats->battle_id);
        battle_unit_store_entd_flags_without_control_flag(unit);
        if (g_battle_menu_status_requested != 0 && !(unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
            && g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            main_sound_play_sfx(MAIN_SFX_STATUS_MENU_REQUEST);
            action_open_status_menu_at_list_start();
            action_show_next_at_list_entry();
            battle_menu_set_next_script_action_menus();
            return;
        }
        g_action_post_action = 0;
        if (g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY
            || g_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
        if (battle_state_try_enter_scenario_event(unit) == 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xff, 0xff);
            g_battle_game_state = BATTLE_GAME_STATE_CHANGE_TURN;
            ai_init_selected_action();
            g_battle_menu_status_enabled = 1;
        } else {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
        g_battle_menu_status_requested = 0;
    } else {
        battle_state_set_free_cursor();
    }
}
