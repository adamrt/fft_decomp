#include "fft/battle_action.h"
#include "fft/battle_gfx.h"

/* Enables the status bubble, restarting its timer, for a unit with a status that
 * shows a bubble (BATTLE_UNIT_STATUS_BUBBLE_MASK) and for the acting unit;
 * disables it for any other unit. */
void battle_gfx_update_status_bubble_graphic_trigger(battle_unit_t* unit) {
    if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_BUBBLE_MASK) != 0
        || unit->runtime_id == g_battle_unit_source_runtime_id) {
        unit->status_bubble_active = 1;
        unit->status_bubble_timer = 0;
    } else {
        unit->status_bubble_active = 0;
    }
}
