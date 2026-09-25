#include "fft/battle_effect.h"
#include "psx/types.h"

void battle_effect_set_misc_data_pointers(s16 effect_id) {
    effect_file_header_t* header;
    u8* effect_misc_data;

    header = g_battle_effect_data_ptrs[effect_id];
    effect_misc_data = (u8*)header + header->misc_data;
    g_battle_effect_misc_data = (effect_misc_data_t*)effect_misc_data;
    g_battle_effect_timing_channels = (battle_effect_tick_channels_t*)(effect_misc_data + 8);
}
