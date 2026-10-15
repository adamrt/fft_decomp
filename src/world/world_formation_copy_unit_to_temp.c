#include "fft/main.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Copy a formation unit into the shared temporary record.
 *
 * The snapshot caller supplies the preview index as a second argument; this
 * routine ignores it and always uses g_world_formation_temp_unit.
 */
void world_formation_copy_unit_to_temp(s16 index, s32 unused_slot) {
    bcopy(g_world_unit_pointers[index], g_world_formation_temp_unit, sizeof(world_unit_t));
}
