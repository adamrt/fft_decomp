#include "fft/battle_camera.h"
#include "fft/battle_unit.h"

/* Projects a unit's position to display coordinates (x, then y).
 *
 * The missing-unit test compares with -1, but
 * battle_unit_get_world_coords_by_runtime_id returns 0 for a missing unit, so the
 * projection then reads address 0. */
void unit_project_to_screen(u32 runtime_id, SVECTOR* screen_coords) {
    SVECTOR* world_coords;

    world_coords = battle_unit_get_world_coords_by_runtime_id(runtime_id);
    if (world_coords != (SVECTOR*)-1) {
        camera_project_world_coords_to_screen(world_coords, screen_coords);
    }
}
