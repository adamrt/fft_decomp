#include "fft/main.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Find the current formation-list index for a roster slot.
 *
 * Narrow the requested slot to s16. On a miss, return the number of entries searched.
 */
s32 world_formation_find_index_by_roster_slot(s16 roster_slot) {
    s32 index;

    for (index = 0; index < g_world_formation_record_count; index++) {
        if (g_world_unit_pointers[index]->identity.roster_slot == roster_slot) {
            break;
        }
    }
    return index;
}
