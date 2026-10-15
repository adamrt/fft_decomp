#include "fft/battle.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Clears g_battle_enemy_level_sum, then runs battle_unit_init_deployed_units_data
 * as the red team's debug deployment. The second argument is ignored. */
void battle_unit_init_deployed_units_data_for_debug_red_team(unit_deployment_coordinates_t* formation, s32 unused_1) {
    g_battle_enemy_level_sum = 0;
    battle_unit_init_deployed_units_data(formation, 1);
}
