#include "fft/battle.h"

void battle_status_enable_special_flags(s32 status_id, s32 enabled, s32 misc_unit_id) {
    if (g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
        battle_status_queue_misc_graphics_flag_change(status_id, enabled, misc_unit_id);
    }
}
