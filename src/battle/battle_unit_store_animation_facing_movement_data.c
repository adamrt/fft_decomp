#include "fft/battle.h"

void battle_unit_store_animation_facing_movement_data(s32 animation, s32 facing, battle_unit_misc_data_t* unit) {
    battle_unit_misc_data_t* misc = unit;
    animation++;
    misc->requested_animation = animation;
    misc->facing = facing;
    misc->movement.word |= 0x20000000;
}
