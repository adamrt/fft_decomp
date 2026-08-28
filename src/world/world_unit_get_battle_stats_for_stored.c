#include "fft/battle.h"
#include "fft/world.h"

battle_stats_t* world_unit_get_battle_stats_for_stored(void) {
    return battle_unit_get_stats_from_battle_id(g_world_unit_view_battle_id);
}
