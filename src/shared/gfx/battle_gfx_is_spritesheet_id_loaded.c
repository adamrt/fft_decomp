#include "fft/battle_gfx.h"

s32 battle_gfx_is_spritesheet_id_loaded(s16 spritesheet_id) {
    s32 slot;

    for (slot = 0; slot < 9; slot++) {
        if (g_battle_gfx_spritesheet_slots[slot].spritesheet_id == spritesheet_id) {
            return 1;
        }
    }
    return 0;
}
