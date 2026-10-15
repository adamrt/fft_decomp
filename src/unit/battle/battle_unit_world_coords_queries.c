/* Unit count, world/tile position lookups by id, screen projection and walk-to. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

s32 battle_unit_count(void) {
    s32 count = 0;
    battle_unit_t* unit = g_battle_unit_list_head;
    while (unit != 0) {
        unit = unit->previous;
        count++;
    }
    return count;
}

/* The integer world position of the unit with this runtime id, or 0 for a missing unit.
 * unit_copy_world_coords and unit_project_to_screen test
 * the result against -1 instead. */
SVECTOR* battle_unit_get_world_coords_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* The integer world position of the unit in slot battle_id, or 0. No retail code calls
 * it. */
SVECTOR* battle_unit_get_world_coords_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/*
 * Store a unit's tile-center coordinates and calculated screen Z.
 *
 * Return 0 when the battle ID has no renderer-side runtime record, otherwise 1.
 * The original performs a tile lookup whose returned pointer is unused; keep
 * that call because it is part of the target instruction stream.
 */
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, SVECTOR* coords) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit != 0) {
        map_get_tile_data_pointer(unit->stats->x, unit->stats->position.y, unit->stats->position.higher_elevation);
        coords->vx = unit->stats->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vz = unit->stats->position.y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vy = battle_gfx_calculate_world_height_from_battle_tile(unit);
        return 1;
    }
    return 0;
}

void camera_convert_unit_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    camera_convert_screen_coords_modify_by_1(battle_unit_get_world_coords_by_runtime_id(runtime_id & 0xffff), coords);
}

/* Projects a world position to screen x and y. Rebuilds the camera matrices first
 * and leaves them loaded in the GTE. */
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords) {
    VECTOR transformed_coords;
    s32 result;

    camera_update_matrices(&g_camera_matrix, &g_camera_render_state, &g_camera_current_screen_offset, &g_camera_zoom);
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    RotTrans(world_coords, &transformed_coords, &result);
    screen_coords->vx = transformed_coords.vx;
    screen_coords->vy = transformed_coords.vy;
}

/* Writes a unit's tile as three 16-bit values in the order x, z, y and returns 1. A
 * runtime id without a record raises pointer exception 12 and returns 0. */
s32 battle_unit_get_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    battle_unit_t* unit = g_battle_unit_list_head;

    for (; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == (runtime_id & 0xffff)) {
            coords->vx = unit->map_x;
            coords->vz = unit->map_y;
            coords->vy = unit->map_z;
            return 1;
        }
    }
    main_system_handle_pointer_exception(12);
    return 0;
}

/* Path a unit to a destination tile and start it walking at the given speed. */
s32 move_start_unit_walk_to(s32 runtime_id, map_tile_coordinates_t* destination, s32 flags, s32 jump, s32 speed) {
    battle_unit_t* unit;
    battle_walk_path_t* path;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xFFFF);
    if (unit != 0) {
        path = move_calculate_walkto_pathing(
            flags, jump, unit->map_x, unit->map_y, unit->map_z, destination->x, destination->y, destination->layer);
        if (path != 0) {
            unit->path = *path;
            unit->walk_speed = speed * 2;
            unit->step_speed = 0x2000;
            unit->movement_path_offset = 0;
            battle_unit_dismount_rider_and_update_display(unit);
            return 1;
        }
    } else {
        main_system_handle_pointer_exception(0xC);
    }
    return 0;
}
