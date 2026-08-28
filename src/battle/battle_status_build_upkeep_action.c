#include "fft/battle.h"
#include "fft/status.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * Per-turn status upkeep: pick the reaction the unit's current status set
 * produces this tick and stage it in the unit's action record.
 *
 * Returns 2 for a regenerating heal, 1 for a sleep/charm style roll, 3 when a
 * timed status expires and 0 when nothing happens.
 */
s32 battle_status_build_upkeep_action(s32 unit_id, battle_stats_t* unit) {
    battle_action_data_t* action = &unit->action;
    s32 roll;
    s32 mask;
    u8 counter;

    battle_action_clear_current_data(action);
    roll = rand();
    if ((unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DEAD)]
            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD)))
            == BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)
        && (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_RERAISE)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE))) {
        action->status_removal[0] = 0x20;
        action->status_removal[2] = 0x20;
        action->attack_type = BATTLE_ACTION_TYPE_HP_HEALING | BATTLE_ACTION_TYPE_STATUS_CHANGE;
        action->hp_healing = unit->max_hp / 10;
        return 2;
    }
    if ((unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DEAD)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))
        && g_battle_action_state == 0) {
        if (unit->team_flags & 4) {
            return 0;
        }
        if (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION)) {
            return 0;
        }
        counter = unit->death_counter;
        counter -= 1;
        if (counter != 0xff) {
            unit->death_counter = counter;
            return 0;
        }
        if ((unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_UNDEAD)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD))
            && (roll & 1)) {
            action->status_removal[0] = 0x20;
            action->hp_healing = ((rand() * unit->max_hp) / 0x8000) + 1;
            action->attack_type = BATTLE_ACTION_TYPE_HP_HEALING | BATTLE_ACTION_TYPE_STATUS_CHANGE;
            return 2;
        }
        mask = 0x10;
        if (unit->initial_team_flags & 8) {
            mask = 0x110;
        }
        if (roll & mask) {
            action->status_infliction[0] = 0x40;
            action->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
        action->status_infliction[1] = 1;
        action->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        return 1;
    }
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE)) {
        counter = unit->status_ct[15];
        counter -= 1;
        if (counter == 0) {
            action->status_removal[4] = 1;
            action->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
            if (!(unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_UNDEAD)]
                    & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD))) {
                action->status_infliction[0] = 0x20;
                return 3;
            }
        } else {
            unit->status_ct[15] = counter;
        }
    }
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DEFENDING)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEFENDING)) {
        action->status_removal[0] = 2;
        action->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
    }
    if (unit->brave < 10) {
        action->brave_change = 0x81;
        action->attack_type |= BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    }
    return (action->attack_type != 0) * 2;
}
