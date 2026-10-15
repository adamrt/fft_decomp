#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

void effect_init_flags_section_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_effect_data_ptrs[effect_id];
    g_effect_flags_section = (const effect_flags_section_t*)((u8*)header + header->flags_section);
}
