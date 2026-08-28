#include "fft/battle.h"
#include "fft/status.h"
#include "psx/types.h"

s32 battle_menu_get_unit_action_menu_id(battle_stats_t* unit) {
    battle_stats_t* act;
    s32 flag;
    s32 player_controlled;
    s32 player_flag;
    s32 dont_move;
    s32 can_evade;
    u8 moved;
    u8 acted;

    flag = 0;
    act = battle_unit_find_active_data_pointer();
    if (act != 0) {
        if ((act->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0 || act->auto_battle_setting != 0) {
            flag = 1;
        }
    }
    player_flag = unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    player_controlled = player_flag != 0;
    if (flag != 0) {
        if (player_controlled != 0) {
            return 0x11;
        }
        return 0x12;
    }
    dont_move = unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DONT_MOVE)];
    can_evade = battle_formula_can_unit_evade(unit);
    moved = unit->movement_taken | (dont_move & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_MOVE));
    acted = unit->action_taken | can_evade;
    if (unit->has_turn != 0) {
        if (moved == 0 && acted == 0) {
            return 0;
        }
        if (moved != 0 && acted == 0) {
            return 0xE;
        }
        if (moved == 0 && acted != 0) {
            return 0x13;
        }
        return 0x14;
    }
    if (player_controlled != 0) {
        return 0xF;
    }
    return 0x10;
}
