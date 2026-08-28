#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x41: hit (MA+X)%; forced miss when caster and target share a zodiac sign. */
void battle_formula_41_hit_ma_x_percent_enemy_only(void) {
    if (battle_formula_calculate_magic_accuracy_without_faith() == 0) {
        if (g_battle_action_target->birthday.fields.zodiac == g_battle_action_attacker->birthday.fields.zodiac) {
            battle_formula_force_attack_miss();
        } else {
            battle_formula_apply_status_to_action();
        }
    }
}
