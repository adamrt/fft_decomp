#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_check_dragon(void) {
    if ((u32)(((u8*)g_battle_action_target)[0x15E] - 0xF) >= 2U) {
        battle_formula_force_attack_miss();
    }
}
