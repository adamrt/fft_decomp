#include "fft/battle_status.h"

/* Return 1 when the unit cannot participate in the AT list. */
s32 status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(const unit_stats_t* unit) {
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 1;
    }
    if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    return 0;
}
