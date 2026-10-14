#include "fft/battle_unit.h"

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
