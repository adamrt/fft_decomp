#include "fft/battle_action.h"
#include "psx/types.h"

/* Clear the record's status inflictions and removals, and the shared list of
 * canceled statuses with them. */
void action_clear_status_changes(action_data_t* action) {
    s32 i;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        g_action_canceled_statuses[i] = 0;
        action->status_changes.infliction[i] = 0;
        action->status_changes.removal[i] = 0;
    }
}
