#include "fft/battle_gfx.h"

/* battle_gfx_calculate_world_height_from_coords at the caller's coords (vy
 * holds the map layer), evaluated for the mount when the unit is a rider. A
 * rider's result is moved by -10 plus the mount's float bob offset. */
s32 battle_gfx_calculate_world_height_with_caller_coords(battle_unit_t* unit, const SVECTOR* coords) {
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
    world_height = battle_gfx_calculate_world_height_from_coords(mount, coords);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}
