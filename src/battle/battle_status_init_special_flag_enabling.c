#include "fft/battle.h"
#include "psx/types.h"

/* Enable the renderer status flags for each of a unit's current statuses.
 * Returns -1 for an empty unit slot, otherwise 0. */
s32 battle_status_init_special_flag_enabling(s32 unit_id) {
    battle_stats_t* unit;
    s32 i;

    unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    i = 0;
    do {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            battle_status_queue_misc_graphics_flag_change(BATTLE_STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
        i += 1;
    } while (i < BATTLE_STATUS_COUNT);
    return 0;
}
