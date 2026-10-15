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
