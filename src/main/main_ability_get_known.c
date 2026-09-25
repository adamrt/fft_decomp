#include "fft/main.h"

u32 main_ability_get_known(battle_stats_t* unit, u32 skillset_id) {
    u32 learned_ability_offset = skillset_id * 3;

    return ((u32)unit->learned_abilities[learned_ability_offset] << 16)
        + ((u32)unit->learned_abilities[learned_ability_offset + 1] << 8)
        | unit->learned_abilities[learned_ability_offset + 2];
}
