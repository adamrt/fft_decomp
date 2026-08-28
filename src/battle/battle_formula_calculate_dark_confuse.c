#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_calculate_dark_confuse(void) {
    if (g_battle_action_attacker->status_sets.current[1]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DARKNESS) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CONFUSION))) {
        u8* base_hit = &g_current_ability.base_hit;
        *base_hit = *base_hit >> 1;
    }
    battle_formula_calculate_transparent();
}
