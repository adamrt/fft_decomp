#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/*
 * Per-turn status upkeep: stage the change the unit's current status set
 * produces this tick in the unit's action record.
 *
 * Returns 2 for a Reraise or undead revival and for any other staged change
 * (Defending removed, the Brave-below-10 change), 1 when the death counter
 * runs out and the unit becomes a crystal or treasure, 3 when Death Sentence
 * expires on a unit that is not undead, and 0 when nothing happens.
 */
s32 status_build_upkeep_action(s32 unit_id, unit_stats_t* unit) {
    action_data_t* action = &unit->action;
    s32 roll;
    s32 mask;
    u8 counter;

    action_clear_current_data(action);
    roll = rand();
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
            & (STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_UNDEAD)))
            == STATUS_BYTE_MASK(STATUS_ID_DEAD)
        && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE))) {
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] = STATUS_BYTE_MASK(STATUS_ID_RERAISE);
        action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_STATUS_CHANGE;
        action->hp_healing = unit->max_hp / 10;
        return 2;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && g_action_state == ACTION_STATE_EXECUTE) {
        if (unit->team_flags & BATTLE_TEAM_FLAG_IMMORTAL) {
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
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
            && (roll & 1)) {
            action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
            action->hp_healing = ((rand() * unit->max_hp) / 0x8000) + 1;
            action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_STATUS_CHANGE;
            return 2;
        }
        mask = 0x10;
        if (unit->initial_team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
            mask = 0x110;
        }
        if (roll & mask) {
            action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                = STATUS_BYTE_MASK(STATUS_ID_CRYSTAL);
            action->attack_type = ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
        action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] = STATUS_BYTE_MASK(STATUS_ID_TREASURE);
        action->attack_type = ACTION_TYPE_STATUS_CHANGE;
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
        & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)) {
        counter = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
        counter -= 1;
        if (counter == 0) {
            action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                = STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE);
            action->attack_type = ACTION_TYPE_STATUS_CHANGE;
            if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                    & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))) {
                action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
                return 3;
            }
        } else {
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] = counter;
        }
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)] & STATUS_BYTE_MASK(STATUS_ID_DEFENDING)) {
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)] = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        action->attack_type = ACTION_TYPE_STATUS_CHANGE;
    }
    if (unit->brave < 10) {
        action->brave_change = 0x81;
        action->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    return (action->attack_type != 0) * 2;
}
