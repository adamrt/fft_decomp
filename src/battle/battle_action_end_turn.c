#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_end_turn(s32 unit_id) {
    battle_stats_t* unit;
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
    movement_check = unit->movement_taken | (status4 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_MOVE));
    action_check = (status4 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_ACT)) | unit->action_taken
        | (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING));
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
