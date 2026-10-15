#include "fft/battle_status.h"
#include "fft/main.h"
#include "psx/types.h"

/* Apply the removals of the unit's action result: each removed status leaves
 * inflicted_status, has its CT cleared and, when executing, its graphics flag
 * switched off; then the current set is rebuilt as innate | inflicted. */
void status_set_inflicted_ct_and_transfer_last_used_ct(s32 unit_idx) {
    unit_stats_t* unit;
    s32 i;
    s32 mask;
    s32 idx;

    unit = &g_battle_unit_stats[unit_idx];
    for (i = 0; i < STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (unit->action.status_changes.removal[idx] & mask) {
            unit->inflicted_status[idx] = unit->inflicted_status[idx] & ~mask;
            if (main_status_set_ct(unit, i, 1) == 0) {
                status_enable_special_flags(STATUS_HANDLER_INDEX(i), 0, unit_idx);
            }
        }
    }
    main_status_store_current(unit);
}
