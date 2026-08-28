#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_calculate_hit(void) {
    battle_formula_calculate_final_hit_percent();
    return *(u8*)g_battle_action_target_data == 0;
}
