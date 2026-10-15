#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Pick a unit's idle action menu (battle_menu_id_e), before the rider variants.
 *
 * While the unit taking its turn is not a manually controlled player unit, every
 * unit gets an inspect menu: PLAYER_UNIT_INSPECT or OTHER_UNIT_INSPECT. Otherwise
 * the unit with the turn gets one of the four TURN menus, by whether its move is
 * gone (moved, or Don't Move) and its action is gone (acted, or any action
 * restriction), and any other unit gets PLAYER_UNIT or OTHER_UNIT.
 *
 * The local can_evade holds the action restriction. */
battle_menu_id_e battle_menu_get_unit_action_menu_id(const unit_stats_t* unit) {
    unit_stats_t* act;
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
        if ((act->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0
            || act->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            flag = 1;
        }
    }
    player_flag = unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    player_controlled = player_flag != 0;
    if (flag != 0) {
        if (player_controlled != 0) {
            return BATTLE_MENU_ID_PLAYER_UNIT_INSPECT;
        }
        return BATTLE_MENU_ID_OTHER_UNIT_INSPECT;
    }
    dont_move = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)];
    can_evade = battle_unit_get_action_restriction(unit);
    moved = unit->movement_taken | (dont_move & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    acted = unit->action_taken | can_evade;
    if (unit->has_turn != 0) {
        if (moved == 0 && acted == 0) {
            return BATTLE_MENU_ID_TURN;
        }
        if (moved != 0 && acted == 0) {
            return BATTLE_MENU_ID_TURN_MOVED;
        }
        if (moved == 0 && acted != 0) {
            return BATTLE_MENU_ID_TURN_ACTED;
        }
        return BATTLE_MENU_ID_TURN_MOVED_ACTED;
    }
    if (player_controlled != 0) {
        return BATTLE_MENU_ID_PLAYER_UNIT;
    }
    return BATTLE_MENU_ID_OTHER_UNIT;
}
