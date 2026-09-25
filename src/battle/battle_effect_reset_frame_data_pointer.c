#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_reset_frame_data_pointer(void) {
    g_battle_effect_frame_data = g_battle_effect_frame_data_start;
}
