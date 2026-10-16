#include "fft/battle_unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Widens a unit's integer world position 16-bit values into three 32-bit components. A missing unit is
 * not tested for: the copy then reads address 0. */
void unit_copy_world_coords_to_vector(u32 runtime_id, VECTOR* destination) {
    SVECTOR* source;

    source = battle_unit_get_world_coords_by_runtime_id(runtime_id);
    destination->vx = source->vx;
    destination->vy = source->vy;
    destination->vz = source->vz;
}
