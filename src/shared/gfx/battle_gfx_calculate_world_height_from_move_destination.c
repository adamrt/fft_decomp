#include "fft/battle_gfx.h"
#include "psx/types.h"

/* battle_gfx_calculate_world_height_from_coords at the center of the movement
 * destination tile of the unit, or of its mount for a rider. A rider's result is
 * moved by -10 plus the mount's float bob offset. */
s32 battle_gfx_calculate_world_height_from_move_destination(battle_unit_t* unit) {
    SVECTOR coord;
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
    coord.vx = mount->movement.destination_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vz = mount->movement.destination_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->movement.destination_z;
    world_height = battle_gfx_calculate_world_height_from_coords(mount, &coord);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}
