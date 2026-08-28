#include "fft/main_unit.h"

/* Sets the unit's charging/jump/defending/performing status bit for
 * `action_state` (clearing the other three) and notifies the battle status
 * handlers about every bit that changed.  0xff keeps performing only when the
 * unit is already performing. */
void main_status_set_action_state(battle_stats_t* unit, u8 action_state) {
    s32 old_status;
    s32 misc_unit_id;
    u8 status;
    u8 cleared;
    s32 status_id;
    u32 mask;
    s32 changed;
    s32 transition;

    old_status = unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CHARGING)];
    misc_unit_id = unit->misc_unit_id;
    if (action_state == MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING) {
        action_state = (old_status & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING)) << 3;
    }
    switch (action_state) {
    case MAIN_UNIT_ACTION_STATE_NONE:
        status = 0;
        unit->charged_ability_ct = 0xff;
        break;
    case MAIN_UNIT_ACTION_STATE_CHARGING:
        status = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING);
        break;
    case MAIN_UNIT_ACTION_STATE_JUMPING:
        status = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP);
        break;
    case MAIN_UNIT_ACTION_STATE_DEFENDING:
        status = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEFENDING);
        unit->charged_ability_ct = 0xff;
        break;
    case MAIN_UNIT_ACTION_STATE_PERFORMING:
        status = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING);
        main_unit_copy_last_ability_ct(unit);
        unit->charged_ability_ct = unit->ability_ct;
        break;
    }
    cleared = status
        ^ (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)
            | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEFENDING)
            | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING));
    main_status_change_unit(unit, BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CHARGING), status, MAIN_STATUS_ADD);
    main_status_change_unit(unit, BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CHARGING), cleared, MAIN_STATUS_REMOVE);

    mask = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING);
    status_id = BATTLE_STATUS_HANDLER_INDEX(BATTLE_STATUS_ID_CHARGING);
    transition = old_status ^ unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CHARGING)];
    /* Keep the xor result live through a distinct allocation.  GCC otherwise
     * coalesces it directly into $s2; the target uses $v0 then moves to $s2. */
    asm volatile("" : : "r"(transition));
    changed = transition;
    for (; status_id <= BATTLE_STATUS_HANDLER_INDEX(BATTLE_STATUS_ID_PERFORMING); status_id++) {
        if (changed & mask) {
            if ((old_status & mask) != 0) {
                battle_status_enable_special_flags(status_id, 0, misc_unit_id);
            } else {
                battle_status_enable_special_flags(status_id, 1, misc_unit_id);
            }
        }
        mask >>= 1;
    }
}
