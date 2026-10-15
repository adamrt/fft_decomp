/* Status-set changes, action state bits and casualty counters. */

#include "fft/battle_status.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/* Increment the battle casualty or injured counter for a unit, capped at 9999. */
void main_unit_increase_casualty_counters(unit_stats_t* unit) {
    s32 id;
    s32 count;

    id = (unit->initial_team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE ? EVENT_SCRIPT_VAR_INJURED
                                                                           : EVENT_SCRIPT_VAR_CASUALTIES;
    count = script_get_variable(id);
    if (count < 0x270F) {
        count++;
    }
    script_set_variable(id, count);
}

/* Add, remove, or replace a status-set flag and refresh the unit's current status. */
void main_status_change_unit(unit_stats_t* unit, s32 status_byte_index, u8 status_flag, s32 change_type) {
    u8 updated;

    /* An enum selector removes the native signed range test. */
    switch (change_type) {
    case MAIN_STATUS_ADD:
        updated = unit->inflicted_status[status_byte_index] | status_flag;
        break;
    case MAIN_STATUS_REMOVE:
        updated = unit->inflicted_status[status_byte_index] & ~status_flag;
        break;
    case MAIN_STATUS_REPLACE:
        updated = status_flag;
        break;
    }

    unit->inflicted_status[status_byte_index] = updated;
    unit->status_sets.current[status_byte_index] = updated | unit->status_sets.innate[status_byte_index];
}

/* Recompute the current status set as innate | inflicted. */
void main_status_store_current(unit_stats_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->status_sets.current[status_set]
            = unit->status_sets.innate[status_set] | unit->inflicted_status[status_set];
    }
}

/* Charge time of the unit's last ability: the low seven bits of the ability
 * record's CT byte. */
void main_unit_copy_last_ability_ct(unit_stats_t* unit) {
    unit->ability_ct = g_main_ability_range_data[unit->action_command.ability_id].ct & 0x7f;
}

/* Sets the unit's charging/jump/defending/performing status bit for
 * `action_state` (clearing the other three) and notifies the battle status
 * handlers about every bit that changed.  0xff keeps performing only when the
 * unit is already performing. */
void main_status_set_action_state(unit_stats_t* unit, u8 action_state) {
    u8 old_status;
    s32 battle_id;
    u8 status;
    u8 cleared;
    s32 status_id;
    u8 mask;
    u8 changed;

    old_status = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    battle_id = unit->battle_id;
    if (action_state == MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING) {
        action_state = (old_status & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) << 3;
    }
    switch (action_state) {
    case MAIN_UNIT_ACTION_STATE_NONE:
        status = 0;
        unit->charged_ability_ct = 0xff;
        break;
    case MAIN_UNIT_ACTION_STATE_CHARGING:
        status = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
        break;
    case MAIN_UNIT_ACTION_STATE_JUMPING:
        status = STATUS_BYTE_MASK(STATUS_ID_JUMP);
        break;
    case MAIN_UNIT_ACTION_STATE_DEFENDING:
        status = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        unit->charged_ability_ct = 0xff;
        break;
    case MAIN_UNIT_ACTION_STATE_PERFORMING:
        status = STATUS_BYTE_MASK(STATUS_ID_PERFORMING);
        main_unit_copy_last_ability_ct(unit);
        unit->charged_ability_ct = unit->ability_ct;
        break;
    }
    cleared = status
        ^ (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
            | STATUS_BYTE_MASK(STATUS_ID_DEFENDING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    main_status_change_unit(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), status, MAIN_STATUS_ADD);
    main_status_change_unit(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), cleared, MAIN_STATUS_REMOVE);

    changed = old_status ^ unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    mask = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
    for (status_id = STATUS_HANDLER_INDEX(STATUS_ID_CHARGING); status_id <= STATUS_HANDLER_INDEX(STATUS_ID_PERFORMING);
        status_id++) {
        if (changed & mask) {
            if ((old_status & mask) != 0) {
                status_enable_special_flags(status_id, 0, battle_id);
            } else {
                status_enable_special_flags(status_id, 1, battle_id);
            }
        }
        mask >>= 1;
    }
}

void main_system_handle_battle_load_exception(s32 value) {
    main_system_handle_malloc_exception(3, value);
}
