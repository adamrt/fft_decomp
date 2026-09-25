#include "fft/battle.h"
#include "psx/types.h"

/* Excludes absent, inert, mounted, or hidden enemies from ordinary AI targeting.
 *
 * The native code deliberately uses only 4 bits for the rider's partner ID,
 * unlike other mount consumers' 5-bit mask. A Transparent rider is admitted
 * only when its mount is not also Transparent. */
s32 battle_ai_can_unit_be_targeted_cryst_trea_mount_trans(s32 unit_idx) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit;
    s32 mount;

    unit = &g_battle_unit_stats[unit_idx];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (*(u16*)&unit->status_sets.current[0]
        & (BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_CRYSTAL)
            | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_TREASURE))) {
        return 1;
    }
    mount = unit->mount_info;
    if (mount & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    if (ai->unit_decisions[unit_idx].enemy_flag == 0) {
        return 0;
    }
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_TRANSPARENT)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT)) {
        if (mount & BATTLE_MOUNT_INFO_FLAG_RIDER) {
            if (g_battle_unit_stats[mount & 0xf]
                    .status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_TRANSPARENT)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT)) {
                goto ret0; /* a direct return folds this test into sltu */
            }
            return 1;
        }
        return 1;
    }
    return 0;
ret0:
    return 0;
}
