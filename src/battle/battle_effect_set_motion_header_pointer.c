#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_set_motion_header_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_battle_effect_data_ptrs[effect_id];
    g_battle_effect_motion_header = (u8*)header + header->motion_header;
}
