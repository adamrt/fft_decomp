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

void effect_set_frame_data_pointer(s16 effect_id) {
    effect_file_header_t* header;
    effect_sprite_section_t* section;

    header = g_effect_data_ptrs[effect_id];
    section = (effect_sprite_section_t*)((u8*)header + header->frame_data);
    g_effect_frame_data = (u8*)section->groups;
}
