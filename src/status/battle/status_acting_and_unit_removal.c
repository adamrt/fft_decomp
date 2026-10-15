/* Acting toggle, Defending, removal marking and move/act restore. */

#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/unit.h"
#include "psx/types.h"

void status_enable_disable_acting(unit_stats_t* unit) {
    main_status_set_action_state(unit, 0);
}

s32 status_inflict_defending_to_battle_id(s32 unit_id) {
    main_status_set_action_state(&g_battle_unit_stats[unit_id], STATUS_HANDLER_INDEX(STATUS_ID_DEFENDING));
    return 0;
}

/* Marks a present unit that has a formation index for removal: the slot is
 * emptied and its existence set to pending removal. Returns -1 when the slot is
 * empty or has no formation index, else 0. No retail code calls it. */
s32 battle_unit_check_exists(s32 unit_id) {
    unit_stats_t* unit;

    unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (unit->formation_index == BATTLE_FORMATION_INDEX_NONE)
        return -1;
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}

/* Give the unit its move and its action back. Nothing in the game calls it. */
void action_activate_move_act(unit_stats_t* unit) {
    unit->movement_taken = 0;
    unit->action_taken = 0;
}
