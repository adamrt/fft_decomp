#include "fft/battle.h"
#include "fft/battle_move.h"

enum {
    LEVEL_UP_ANIMATION_SUBMERGED = 0x09,
    LEVEL_UP_ANIMATION_WALKING = 0x1c,
    LEVEL_UP_ANIMATION_SMALL_HOPS = 0x1d,
};

void battle_unit_set_level_up_animation(battle_unit_misc_data_t* unit) {
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        battle_unit_store_animation_facing(LEVEL_UP_ANIMATION_SMALL_HOPS, (s16)unit->facing, unit);
        return;
    }
    switch ((u8)battle_move_validate_float_fly(unit)) {
    case 0:
        battle_unit_store_animation_facing(LEVEL_UP_ANIMATION_WALKING, (s16)unit->facing, unit);
        break;
    case 1:
        battle_unit_store_animation_facing(LEVEL_UP_ANIMATION_SMALL_HOPS, (s16)unit->facing, unit);
        break;
    case 2:
        battle_unit_store_animation_facing(LEVEL_UP_ANIMATION_SUBMERGED, (s16)unit->facing, unit);
        break;
    }
}
