#include "fft/battle.h"

u32 main_ability_get_known(battle_stats_t* unit, u32 skillset_id) {
    u32 index = skillset_id * 3;

    return ((u32)unit->learned_abilities[index] << 16) + ((u32)unit->learned_abilities[index + 1] << 8)
        | unit->learned_abilities[index + 2];
}
