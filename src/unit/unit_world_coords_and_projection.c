/* World coordinate copies and screen projection by runtime id. */

#include "fft/battle_camera.h"
#include "fft/battle_unit.h"

/* Copy integer world coordinates into the shared signed 16-bit triple.
 * The -1 test is retained although the getter returns null for a missing unit.
 */
SVECTOR* unit_copy_world_coords_to_scratch(u32 runtime_id) {
    SVECTOR* source;
    SVECTOR* dest;

    source = battle_unit_get_world_coords_by_runtime_id(runtime_id);

    if (source != (SVECTOR*)-1) {
        dest = &g_unit_runtime_coords;
        dest->vx = source->vx;
        g_unit_runtime_coords.vy = source->vy;
        g_unit_runtime_coords.vz = source->vz;
        return dest;
    }
    return (SVECTOR*)-1;
}

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

/* Copies a unit's integer world position as three 16-bit values.
 *
 * The missing-unit test compares with -1, but
 * battle_unit_get_world_coords_by_runtime_id returns 0 for a missing unit, so the
 * copy then reads address 0. */
void unit_copy_world_coords(u32 runtime_id, SVECTOR* world_coords) {
    SVECTOR* source;

    source = battle_unit_get_world_coords_by_runtime_id(runtime_id);
    if (source != (SVECTOR*)-1) {
        world_coords->vx = source->vx;
        world_coords->vy = source->vy;
        world_coords->vz = source->vz;
    }
}
