#include "fft/battle.h"

/* Index of the first present unit standing on the current action's target
 * panel, or BATTLE_UNIT_ID_NONE if none. */
s32 battle_ai_has_any_unit_on_target_panel(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 i;
    battle_stats_t* unit;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->x == ai->current_action.target_x
            && unit->position.bits.y == ai->current_action.target_y
            && (unit->position.raw >> 15) == ai->current_action.target_elevation) {
            break;
        }
    }
    return i;
}
