#include "fft/battle.h"

battle_stats_t* battle_unit_get_attacker_data_pointer(void) {
    return battle_unit_get_stats_from_battle_id(g_battle_active_turn_unit.battle_id);
}
