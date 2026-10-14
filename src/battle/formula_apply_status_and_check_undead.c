#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "psx/types.h"

/* Apply the ability's status. Returns 1 when it changes something or the
 * target is undead; otherwise the action is canceled as a forced failure and
 * 0 returned. */
s32 formula_apply_status_and_check_undead(void) {
    formula_apply_status();
    if (g_action_target_data->attack_type & ACTION_TYPE_STATUS_CHANGE) {
        return 1;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        return 1;
    }
    formula_nullify_action();
    g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    return 0;
}
