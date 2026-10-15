/* Runtime-id existence test, facing-update suppression for event instructions and the flip set. */

#include "fft/battle_unit.h"
#include "psx/types.h"

s32 battle_unit_has_runtime_id(u32 runtime_id) {
    return battle_unit_get_by_runtime_id(runtime_id & 0xffff) != 0;
}

/* Sets BATTLE_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit and clears its 16-bit sprite
 * display flags. */
void battle_unit_set_facing_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->sprite_display_flags = 0;
        unit->movement.suppress_facing_update = 1;
    }
}

void battle_unit_clear_facing_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_facing_update = 0;
    }
}

void battle_unit_set_horizontal_flip_flag(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->horizontal_flip_flag = BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
    }
}
