#include "fft/battle.h"

/* Return 1 when the unit cannot participate in the AT list. */
s32 battle_status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(const battle_stats_t* unit) {
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (unit->status_sets.current[0]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))) {
        return 1;
    }
    if (unit->status_sets.current[1]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE))) {
        return 1;
    }
    if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    return 0;
}
