#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_calculate_concentrate(void) {
    if (g_battle_action_attacker->support_abilities[1] & BATTLE_SUPPORT_SET_2_CONCENTRATE) {
        main_util_clear_byte_data(&g_current_ability.accessory_evade, 4);
    }
}
