#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_text.h"
#include "psx/types.h"

/* Enter the post-action report state for the source unit's current phase.
 *
 * Phase 0 reports a level up, phase 1 a job level up, and phase 2 looks for a
 * unit that learns the used ability on hit; without one the phase advances. */
void action_init_learn_ability_on_hit(void) {
    battle_unit_t* unit;
    battle_unit_t* learner;
    u16 ability_id;

    g_battle_game_state = BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT;
    g_main_system_animation_speed = 1;
    unit = battle_unit_get_source();
    battle_menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    text_set_message_duration_frames(0x1E);
    switch (g_action_post_action_display_phase) {
    case BATTLE_POST_ACTION_DISPLAY_LEVEL_UP:
        action_report_level_up(unit);
        break;
    case BATTLE_POST_ACTION_DISPLAY_JOB_LEVEL_UP:
        action_report_job_level_up(unit);
        break;
    case BATTLE_POST_ACTION_DISPLAY_LEARN_ON_HIT:
        g_action_learn_on_hit_unit_id = battle_ability_find_learn_on_hit_unit(unit->stats->battle_id, &ability_id);
        if (g_action_learn_on_hit_unit_id != -1) {
            learner = battle_unit_get_by_battle_id(g_action_learn_on_hit_unit_id);
            battle_menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_LEARNED, ability_id,
                unit->stats->battle_id, 0, learner->stats->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            target_move_cursor_to_unit(learner);
            action_set_at_list_active();
        } else {
            g_action_post_action_display_phase++;
        }
        break;
    }
    target_store_cursor_unit_name_and_data();
}
