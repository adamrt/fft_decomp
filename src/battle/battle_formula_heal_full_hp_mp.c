#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x4A: heal 100% HP and 100% MP. */
void battle_formula_heal_full_hp_mp(void) {
    battle_formula_apply_full_hp_mp_heal();
    battle_formula_apply_undead_reversal();
}
