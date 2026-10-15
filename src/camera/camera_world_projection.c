/* World-to-screen projection with unit tile and walk-to helpers. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* camera_convert_screen_coords_modify_by_1 on a unit's position. An absent
 * unit passes address 0 on. No retail code calls it. */
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
