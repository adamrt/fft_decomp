#include "fft/battle_status.h"
#include "psx/types.h"

/* Enable the renderer status flags for each of a unit's current statuses.
 * Returns -1 for an empty unit slot, otherwise 0. */
s32 status_init_special_flag_enabling(s32 unit_id) {
    unit_stats_t* unit;
    s32 i;

    unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_runtime_graphics_flag_change(STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
    }
    return 0;
}
