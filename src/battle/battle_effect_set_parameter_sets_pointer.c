#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_set_parameter_sets_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_battle_effect_data_ptrs[effect_id];
    g_battle_effect_parameter_sets = (u8*)header + header->parameter_sets + 4;
}
