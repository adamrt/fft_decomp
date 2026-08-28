#include "fft/battle.h"

s32 battle_unit_set_animation_from_facing_hint(u32 misc_id, s32 facing_hint) {
    battle_unit_misc_data_t* unit;
    s32 animation;
    s16 facing;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        animation = unit->encoded_animation >> 1;
        facing = facing_hint << 10;
        unit->facing = facing;
        if (animation < 3) {
            animation = 2;
        }
        battle_unit_store_animation_facing_movement_data(animation, facing, (u8*)unit);
        return misc_id;
    }
    return -1;
}
