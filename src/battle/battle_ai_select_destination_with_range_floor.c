#include "fft/battle_ai.h"

/*
 * Select a destination with the conditional three-tile distance floor.
 *
 * Against an enemy target, targeting flag 0x01 raises smaller limits to 3.
 * Selection still uses the ordinary unrestricted distance-first fallback.
 */
void battle_ai_select_destination_with_range_floor(s32 distance_limit) {
    battle_ai_unit_decision_t* decision = g_battle_ai_data_base.acting_unit_decision;
    if (g_battle_ai_data_base.unit_decisions[decision->main_target_id].enemy_flag != 0
        && (decision->targeting_flags_1 & 1) && distance_limit < 3) {
        distance_limit = 3;
    }
    battle_ai_select_destination_with_nearest_fallback(distance_limit);
}
