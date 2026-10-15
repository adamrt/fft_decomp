#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Resolve a unit's status changes with the action state set to preview (2). */
void status_resolve_unit_changes_in_preview(s32 unit_id, s32 removal_only) {
    g_action_state = ACTION_STATE_PREVIEW;
    status_resolve_unit_changes(unit_id, removal_only);
    g_action_state = ACTION_STATE_EXECUTE;
}
