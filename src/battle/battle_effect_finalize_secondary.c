#include "fft/battle.h"

void battle_effect_finalize_secondary(u8 target_id) {
    g_battle_effect_secondary_data[target_id].phase = BATTLE_SECONDARY_EFFECT_FINALIZING;
}
