#include "fft/battle.h"

void battle_formula_apply_magical_support_status_compatibility(void) {
    battle_formula_apply_magic_attack_up();
    battle_formula_apply_magic_defense_up();
    battle_formula_apply_target_magical_status_xa_modifiers();
    battle_formula_apply_zodiac_compatibility();
}
