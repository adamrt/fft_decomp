#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_init_deployed_units_data_for_debug_red_team(s32 value) {
    g_enemy_level_sum = 0;
    battle_unit_init_deployed_units_data((battle_deployed_coords_t*)value, 1);
}
