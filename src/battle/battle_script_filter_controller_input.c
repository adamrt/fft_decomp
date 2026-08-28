#include "fft/battle.h"
#include "psx/types.h"

void battle_script_filter_controller_input(u32* input) {
    g_battle_script_unfiltered_controller_input = *input;
    battle_script_apply_input_overrides(input);
}
