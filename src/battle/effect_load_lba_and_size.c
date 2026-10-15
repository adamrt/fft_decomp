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

void effect_load_lba_and_size(s32 effect_id, s32* lba, s32* byte_length) {
    u32 stack_padding[2];

    *lba = g_effect_disc_entries[effect_id].sector;
    *byte_length = g_effect_disc_entries[effect_id].size;
}
