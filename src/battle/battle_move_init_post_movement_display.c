#include "fft/battle.h"
#include "fft/battle_move.h"

void battle_move_init_post_movement_display(void) {
    battle_stats_t* stats;

    battle_state_stop_game_flow();
    stats = battle_unit_get_casting_misc_data()->battle_data;
    if (stats != 0) {
        g_battle_move_find_result = battle_move_set_target_for_mounted_unit_and_find_item(stats);
    } else {
        g_battle_move_find_result = 0;
    }
    g_battle_action_post_action_display_phase = 0;
    battle_move_start_next_post_movement_step();
    if (g_battle_move_find_result != 0) {
        battle_action_clear_at_list_active();
    }
    g_battle_state_animation_continue_check = 0;
}
