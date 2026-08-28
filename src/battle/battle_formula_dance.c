#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_dance(void) {
    battle_formula_force_sleeping_target_miss();
    if (g_battle_action_target_data->hit != 0) {
        if (battle_formula_calculate_dance_song_hit() == 0) {
            battle_formula_calculate_base_xa();
            battle_formula_apply_dance_abilities();
        }
    }
}
