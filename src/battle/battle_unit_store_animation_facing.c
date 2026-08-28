#include "fft/battle.h"

void battle_unit_store_animation_facing(u32 animation, s32 facing, battle_unit_misc_data_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
}
