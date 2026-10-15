#include "fft/battle_gfx.h"
#include "psx/types.h"

/* battle_gfx_calculate_world_height_from_coords at the current x and map-depth
 * position of the unit, or of its mount for a rider, on the mount's map layer. A
 * rider's result is moved by -10 plus the mount's float bob offset. */
s32 battle_gfx_calculate_world_height_from_world_coords(battle_unit_t* unit) {
    SVECTOR coord;
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
    /* The target loads the s16 world coordinates unsigned (lhu). */
    coord.vx = mount->world_coords.vx;
    coord.vz = mount->world_coords.vz;
    coord.vy = mount->map_z;
    world_height = battle_gfx_calculate_world_height_from_coords(mount, &coord);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}
