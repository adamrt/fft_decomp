#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* End the unit's turn: CT gains 20 when it neither moved nor has Don't Move,
 * and 20 when it neither acted nor has Don't Act or Performing; the result is
 * capped at 60. Always returns 1. */
s32 action_end_turn(s32 unit_id) {
    unit_stats_t* unit;
    u16 ct;
    u8 status4;
    u8 movement_check;
    u8 action_check;
    u32 result;

    unit = &g_battle_unit_stats[unit_id];
    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE)
        return 1;
    if (unit->has_turn == 0)
        return 1;
    ct = unit->ct;
    status4 = unit->status_sets.current[4];
    movement_check = unit->movement_taken | (status4 & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    action_check = (status4 & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT)) | unit->action_taken
        | (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)] & STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    if (movement_check == 0) {
        ct += 0x14;
    }
    result = ct & 0xFFFF;
    if (action_check == 0) {
        ct += 0x14;
        result = ct & 0xFFFF;
    }
    if (result >= 0x3D)
        ct = 0x3C;
    unit->ct = ct;
    unit->has_turn = 0;
    return 1;
}
