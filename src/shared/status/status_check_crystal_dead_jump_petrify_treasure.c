#include "fft/battle_status.h"

/* 1 for an empty slot or a unit that is a crystal, dead, in the air from Jump,
 * petrified or a treasure; otherwise 0. */
s32 status_check_crystal_dead_jump_petrify_treasure(const unit_stats_t* unit) {
    s32 result;
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return 1;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP)))
        return 1;
    result = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE));
    if (result != 0)
        result = 1;
    return result;
}
