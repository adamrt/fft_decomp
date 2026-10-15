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

void effect_set_timeline_pointers(s16 effect_id) {
    effect_file_header_t* header;
    effect_timeline_t* timeline;

    header = g_effect_data_ptrs[effect_id];
    timeline = (effect_timeline_t*)((u8*)header + header->timeline);
    g_effect_timeline = timeline;
    g_effect_child_timeline = &timeline->child;
}
