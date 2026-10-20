/* Copying a unit's world position into a vector. */

#include "fft/unit_runtime.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Widens a unit's integer world position 16-bit values into three 32-bit components. A missing unit is
 * not tested for: the copy then reads address 0. */
void unit_world_vector_copy(u32 runtime_id, VECTOR* destination) {
    SVECTOR* source = unit_runtime_world_coords_get(runtime_id);

    destination->vx = source->vx;
    destination->vy = source->vy;
    destination->vz = source->vz;
}
