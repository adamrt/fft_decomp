#include "fft/battle_action.h"
#include "fft/battle_status.h"

/* Passes a status change on to the unit's display flags
 * (status_queue_runtime_graphics_flag_change), but only while an action is
 * executing: previews and AI simulations change no display state.
 *
 * status_id is the status handler index (status id + 1). */
void status_enable_special_flags(s32 status_id, s32 enabled, s32 battle_id) {
    if (g_action_state == ACTION_STATE_EXECUTE) {
        status_queue_runtime_graphics_flag_change(status_id, enabled, battle_id);
    }
}
