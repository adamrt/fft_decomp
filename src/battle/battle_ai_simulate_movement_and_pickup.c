#include "fft/battle_ai.h"

/*
 * Simulate movement and its crystal or treasure benefit.
 *
 * An unchanged coordinate word skips all updates. Crystal recovery clears
 * Critical; a noncritical pickup adds priority unless autobattle_setting is 0x11.
 */
void battle_ai_simulate_movement_and_pickup(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;

    if (ai->current_action.coords.word != ai->acting_unit_coords.word) {
        battle_ai_move_temp_unit_to_coords(&ai->current_action.coords);
        if (ai->crystal_treasure_status == 1) {
            ai->acting_unit->hp = ai->acting_unit->max_hp;
            ai->acting_unit->mp = ai->acting_unit->max_mp;
            ai->acting_unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRITICAL)]
                &= ~BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL);
            ai->acting_unit->inflicted_status[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRITICAL)]
                &= ~BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL);
        }
        if (ai->crystal_treasure_status != 0
            && !(ai->acting_unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRITICAL)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL))
            && ai->autobattle_setting != BATTLE_AI_AUTOBATTLE_RETREAT) {
            ai->current_action.priority += 2;
        }
        ai->acting_unit->movement_taken = 1;
    }
}
