#include "fft/battle_effect.h"
#include "psx/types.h"

void battle_effect_set_frame_data_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_battle_effect_data_ptrs[effect_id];
    g_battle_effect_frame_data = (u8*)header + header->frame_data + 4;
}
