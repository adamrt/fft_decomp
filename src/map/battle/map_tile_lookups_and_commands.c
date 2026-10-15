/* Cursor/unit tile lookups, data load and map object commands. */

#include "fft/battle.h"
#include "fft/battle_map.h"
#include "psx/types.h"

void map_store_selected_tile_coordinates(SVECTOR* destination) {
    main_util_set_svector(destination, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
}

/* Write the map's width and depth in tiles to `destination` as (x, 0, z). */
void map_store_max_coordinates(SVECTOR* destination) {
    main_util_set_svector(destination, g_map_max_x, 0, g_map_max_y);
}

/* The tile a unit stands on, by runtime id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
map_tile_t* map_get_unit_tile_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    return map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
}

/* The tile a unit stands on, by battle id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
map_tile_t* map_get_tile_data_ptr_from_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    return map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
}

/* The tile under a unit's integer world position (28 units per tile) on its map layer. */
map_tile_t* map_get_unit_tile_from_world_coords(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    return map_get_tile_data_pointer(
        unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
}

/* Ask the map loader to load what the current map state needs. 1 when the load
 * started, 0 when a file read is still in progress. */
s32 map_start_data_load(void) {
    return map_load_data(g_map_id, MAP_LOAD_COMMAND_START) != 0;
}

/* Advance the current map's load by one step. Nonzero while it is still loading. */
s32 map_poll_data_load(void) {
    return map_load_data(g_map_id, MAP_LOAD_COMMAND_POLL);
}

/* Event Use3DObject: start state row value_b of map mesh part value_a (map data
 * command 0x80, with flag 1). Always returns 1. */
s32 map_command_set_3d_object_state(s32 value_a, s32 value_b) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, value_a, value_b, 1);
    return 1;
}

/* Return the map mesh part's animation state; zero means its animation has ended. */
s32 map_command_get_3d_object_state(s32 value) {
    return map_dispatch_map_data_command(MAP_DATA_COMMAND_GET_3D_OBJECT_STATE, value, 1, 1);
}

/* Start map texture animation value_a from its first frame (map data command
 * 0x83). value_b goes in the object state argument, which that command does not
 * read. Always returns 1. */
s32 map_command_start_texture_animation(s32 value_a, s32 value_b) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_START_TEXTURE_ANIMATION, value_a, value_b, 1);
    return 1;
}

/* Return whether the map texture animation is still playing. */
s32 map_command_get_texture_animation_active(s32 value) {
    return map_dispatch_map_data_command(MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE, value, 1, 1);
}
