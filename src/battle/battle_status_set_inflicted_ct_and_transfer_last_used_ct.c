#include "fft/main_unit.h"
#include "fft/status.h"
#include "psx/types.h"

void battle_status_set_inflicted_ct_and_transfer_last_used_ct(s32 unit_idx) {
    battle_stats_t* unit;
    s32 i;
    s32 mask;
    s32 idx;

    unit = &g_battle_unit_stats[unit_idx];
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (unit->action.status_removal[idx] & mask) {
            unit->inflicted_status[idx] = (u8)(unit->inflicted_status[idx] & ~mask);
            if (main_status_set_ct(unit, i, 1) == 0) {
                battle_status_enable_special_flags(BATTLE_STATUS_HANDLER_INDEX(i), 0, unit_idx);
            }
        }
    }
    main_status_store_current(unit);
}
