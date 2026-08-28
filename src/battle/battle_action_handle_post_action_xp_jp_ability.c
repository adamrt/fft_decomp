#include "fft/battle.h"
#include "psx/types.h"

void battle_action_handle_post_action_xp_jp_ability(void) {
    battle_unit_misc_data_t* unit;
    battle_stats_t* stats;

    unit = battle_unit_get_source_misc_data();
    battle_unit_sync_all_rider_mount_positions();
    if (unit != 0) {
        stats = unit->battle_data;
        if (stats != 0 && battle_unit_apply_action_rewards(stats->misc_unit_id, &unit->action_rewards) == -1) {
            unit->action_rewards.job_level_for_display = 0;
            unit->action_rewards.level_for_display = 0;
            unit->action_rewards.earned_jp = 0;
            unit->action_rewards.earned_experience = 0;
        }
    }
    battle_gfx_init_earned_exp_jp_display(unit);
    g_battle_action_post_action_display_phase = 0;
    battle_target_move_cursor_to_unit(unit);
    battle_action_init_learn_ability_on_hit();
    g_battle_action_post_action = 0;
    g_battle_state_animation_continue_check = 0;
}
