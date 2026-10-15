#include "fft/battle_gfx.h"
#include "psx/types.h"

/*
 * Calculate world height at the grounded unit's map-tile center.
 *
 * A mounted rider uses the mount's map position, then applies its mounted
 * height offset and the original -10 adjustment.
 */
s32 battle_gfx_calculate_world_height_from_battle_tile(battle_unit_t* unit) {
    SVECTOR coord;
    unit_stats_t* stats;
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
    stats = mount->stats;
    coord.vx = stats->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    stats = mount->stats;
    coord.vz = stats->position.y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->map_z;
    world_height = battle_gfx_calculate_world_height_from_coords(mount, &coord);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}
