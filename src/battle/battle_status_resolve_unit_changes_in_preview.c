#include "fft/battle.h"
#include "psx/types.h"

/* Deliberately argument-free: the target sets up no arguments before this
 * call. The real `(s32 unit_id, s32 removal_only)` prototype in fft/battle.h is
 * reached through a function-pointer cast at the call, so the header can be
 * included here; the preview/execute action-state values remain literal. */

void battle_status_resolve_unit_changes_in_preview(void) {
    g_battle_action_state = 2;
    ((void (*)(void))battle_status_resolve_unit_changes)();
    g_battle_action_state = 0;
}
