/* Copying a unit's world position into a vector. */

#include "fft/map_unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Widens a unit's integer world position 16-bit values into three 32-bit components. A missing unit is
 * not tested for: the copy then reads address 0. */
void map_unit_copy_world_vector(u32 map_unit_id, VECTOR* destination) {
    SVECTOR* source = map_unit_get_world_coords(map_unit_id);

    destination->vx = source->vx;
    destination->vy = source->vy;
    destination->vz = source->vz;
}
