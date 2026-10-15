#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Returns the BATTLE_TURN_STATUS_* flags that decide how a unit takes part in the
 * turn order.
 *
 * CT_FROZEN alone for an empty slot, a unit with a status of the freeze-CT set or
 * a ridden mount; INCAPACITATED | DEAD for a dead unit; otherwise INCAPACITATED |
 * SLEEP for Sleep plus the HASTE and SLOW bits. The retail status data puts Sleep
 * in the freeze-CT set, which leaves the SLEEP result unreachable. */
s32 status_calculate_turn_flags(const unit_stats_t* unit) {
    s32 status1;
    s32 status4;
    s32 status5;
    s32 flags;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_FREEZE_CT) != 0) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    status1 = unit->status_sets.current[0];
    status4 = unit->status_sets.current[3];
    status5 = unit->status_sets.current[4];
    if ((status1 & STATUS_BYTE_MASK(STATUS_ID_DEAD)) != 0) {
        return BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_DEAD;
    }
    flags = -((status5 & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) != 0)
        & (BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_SLEEP);
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_HASTE)) != 0) {
        flags |= BATTLE_TURN_STATUS_HASTE;
    }
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_SLOW)) != 0) {
        flags |= BATTLE_TURN_STATUS_SLOW;
    }
    return flags;
}
