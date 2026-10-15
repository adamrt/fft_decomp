#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Store the tile a strike lands on in the strike work record: the random-fire
 * tile picked for this strike, else the target unit's tile and level, else the
 * action's target tile. skip_empty_tile_effect is cleared. */
void target_set_coordinates_for_ability(const unit_stats_t* acting, battle_strike_work_t* out) {
    unit_stats_t* target;

    out->skip_empty_tile_effect = 0;
    if (g_action_current_ability.random_fire_flag != 0) {
        out->target_new_x = g_action_current_ability.target_new_x;
        out->target_new_y = g_action_current_ability.target_new_y;
        out->target_new_map_level = g_action_current_ability.target_new_elevation;
    } else if (acting->action_command.targeting_type == BATTLE_TARGETING_TYPE_UNIT) {
        target = &g_battle_unit_stats[acting->action_command.target_id];
        out->target_new_x = target->x;
        out->target_new_y = target->position.y;
        out->target_new_map_level = target->position.higher_elevation;
    } else {
        out->target_new_x = acting->action_command.target_position.vx;
        out->target_new_y = acting->action_command.target_position.vz;
        out->target_new_map_level = acting->action_command.target_position.vy;
    }
}
