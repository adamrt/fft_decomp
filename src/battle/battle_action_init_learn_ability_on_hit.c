#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Enter the post-action report state for the source unit's current phase.
 *
 * Phase 0 reports a level up, phase 1 a job level up, and phase 2 looks for a
 * unit that learns the used ability on hit; without one the phase advances. */
void battle_action_init_learn_ability_on_hit(void) {
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* learner;
    u16 ability_id;

    g_battle_game_state = BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT;
    g_animation_speed = 1;
    unit = battle_unit_get_source_misc_data();
    battle_menu_init_system_function(0xA, 0, unit->battle_data->misc_unit_id, 0, 0);
    battle_text_set_message_duration_frames(0x1E);
    switch (g_battle_action_post_action_display_phase) {
    case 0:
        battle_action_report_level_up(unit);
        break;
    case 1:
        battle_action_report_job_level_up(unit);
        break;
    case 2:
        g_battle_action_post_action_unit_id
            = battle_ability_find_learn_on_hit_unit(unit->battle_data->misc_unit_id, &ability_id);
        if (g_battle_action_post_action_unit_id != -1) {
            learner = battle_unit_get_misc_data_by_battle_id((u16)g_battle_action_post_action_unit_id);
            battle_menu_init_system_function(0xB, ability_id, unit->battle_data->misc_unit_id, 0,
                learner->battle_data->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            battle_target_move_cursor_to_unit(learner);
            battle_action_set_at_list_active();
        } else {
            g_battle_action_post_action_display_phase++;
        }
        break;
    }
    battle_target_store_cursor_unit_name_and_data();
}
