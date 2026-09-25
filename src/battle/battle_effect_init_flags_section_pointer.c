#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_init_flags_section_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_battle_effect_data_ptrs[effect_id];
    g_battle_effect_flags_section = (effect_flags_section_t*)((u8*)header + header->flags_section);
}
