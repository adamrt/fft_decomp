#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_3d_hit_ma_x_percent(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_without_faith() == 0) {
            battle_formula_apply_status_to_action();
        }
    }
}
