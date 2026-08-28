#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_get_random_0_7fff(void) {
    if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
        return 0x4000;
    }
    return rand();
}
