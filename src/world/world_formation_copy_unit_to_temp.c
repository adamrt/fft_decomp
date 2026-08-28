#include "fft/data.h"
#include "fft/world.h"
#include "psx/libc.h"

/*
 * Copy a formation unit into the shared temporary record.
 *
 * The snapshot caller supplies slot 20 as a second argument; this routine
 * ignores it and always uses g_world_formation_temp_unit.
 */
void world_formation_copy_unit_to_temp(s32 index, s32 unused_slot) {
    bcopy(g_world_formation_unit_pointers[(s16)index], g_world_formation_temp_unit, sizeof(world_formation_unit_t));
}
