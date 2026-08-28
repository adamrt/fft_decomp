#include "fft/battle.h"

s32 battle_effect_get_secondary_phase(u8 target_id) {
    return g_battle_effect_secondary_data[target_id].phase;
}
