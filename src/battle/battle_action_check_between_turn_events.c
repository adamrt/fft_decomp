#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/main_runtime.h"
#include "fft/option.h"
#include "psx/types.h"

/* Runs any in-between-turn event, then opens the next acting unit's turn. */
void battle_action_check_between_turn_events(void) {
    battle_unit_misc_data_t* unit;

    if (battle_map_try_start_deep_dungeon_mesh_load() != 0) {
        return;
    }
    battle_action_clear_at_list_id();
    g_animation_speed = 1;
    battle_action_run_between_turn_events();
    unit = battle_unit_get_source_misc_data();
    if (unit == 0) {
        return;
    }
    g_battle_casting_unit_id = unit->unit_id;
    battle_unit_store_entd_flags_without_control_flag(unit);
    if (unit->battle_data != 0) {
        battle_menu_reset_unit_record(unit->battle_data->misc_unit_id);
        battle_unit_store_entd_flags_without_control_flag(unit);
        if (g_battle_menu_status_requested != 0 && !(unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
            && g_action_type == BATTLE_TURN_EVENT_UNIT_READY) {
            battle_unit_update_display_by_misc_id(unit->unit_id);
            main_sound_play_sfx(0x71);
            battle_action_open_status_menu_at_list_start();
            battle_action_is_at_list_unit_charging_ability();
            battle_menu_set_next_script_action_menus();
            return;
        }
        g_battle_action_post_action = 0;
        if (g_action_type == BATTLE_TURN_EVENT_UNIT_READY || g_action_type == BATTLE_TURN_EVENT_NONE) {
            battle_unit_update_display_by_misc_id(unit->unit_id);
        }
        if (battle_menu_init_action_menu(unit) == 0) {
            battle_menu_store_unit_names_and_event_block_data(1, 0xff, 0xff);
            g_battle_game_state = BATTLE_GAME_STATE_CHANGE_TURN;
            battle_ai_init_selected_action();
            g_battle_menu_status_enabled = 1;
        } else {
            battle_unit_update_display_by_misc_id(unit->unit_id);
        }
        g_battle_menu_status_requested = 0;
    } else {
        battle_state_set_free_cursor();
    }
}
