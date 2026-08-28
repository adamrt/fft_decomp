#include "fft/main_unit.h"
#include "psx/types.h"

s32 battle_status_check_unit(battle_stats_t* unit) {
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
    if ((status1 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)) != 0) {
        return BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_DEAD;
    }
    flags = -(s32)((status5 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLEEP)) != 0)
        & (BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_SLEEP);
    if ((status4 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_HASTE)) != 0) {
        flags |= BATTLE_TURN_STATUS_HASTE;
    }
    if ((status4 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLOW)) != 0) {
        flags |= BATTLE_TURN_STATUS_SLOW;
    }
    return flags;
}
