#include "fft/battle_status.h"
#include "fft/main.h"

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
