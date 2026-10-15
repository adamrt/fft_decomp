#include "fft/battle_ai.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"

/*
 * Queue the renderer-side status flags associated with a simulation status.
 *
 * The status argument is the callback-table index (canonical status ID + 1).
 * Crystal removal deliberately stages the Chicken transformation bit, as in
 * the original transition logic. Rebuilding the unit's AI ability data when
 * Frog changes keeps its usable-ability list synchronized with the graphics
 * transition.
 */
s32 status_queue_runtime_graphics_flag_change(s32 status, s32 enabled, s32 battle_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit == 0) {
        return 0;
    }
    switch (enabled) {
    case 0:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_remove_5_6 |= BATTLE_UNIT_STATUS_CHICKEN;
            unit->statuses_to_add_5_6 &= ~BATTLE_UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_remove_5_6 |= BATTLE_UNIT_STATUS_FROG;
            unit->statuses_to_add_5_6 &= ~BATTLE_UNIT_STATUS_FROG;
            ai_init_unit_abilities(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_remove_5_6 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_add_5_6 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_remove_1_4 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_add_1_4 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        }
        break;
    case 1:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_CRYSTAL;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_CRYSTAL;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_CHICKEN;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_FROG;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_FROG;
            ai_init_unit_abilities(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_TREASURE;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_TREASURE;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_add_5_6 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_remove_5_6 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_add_1_4 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_remove_1_4 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        }
        break;
    case 2:
        /* No effect, but the label makes 1 the root of the target's
         * compare tree. */
        break;
    }
    return 1;
}
