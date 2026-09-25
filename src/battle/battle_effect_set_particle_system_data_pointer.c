#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "psx/types.h"

void battle_effect_set_particle_system_data_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_battle_effect_data_ptrs[effect_id];
    g_effect_geometry_table = (effect_geometry_table_t*)((u8*)header + header->particle_system);
}
