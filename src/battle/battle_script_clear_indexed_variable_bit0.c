#include "fft/battle.h"
#include "psx/types.h"

void battle_script_clear_indexed_variable_bit0(void) {
    g_battle_script_variables[g_battle_camera_script_variable_indices[6]] &= ~1;
}
