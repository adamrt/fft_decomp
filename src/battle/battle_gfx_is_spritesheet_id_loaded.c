#include "fft/battle_gfx.h"

s32 battle_gfx_is_spritesheet_id_loaded(s32 spritesheet_id) {
    s32 slot;
    s32 offset;

    slot = 0;
    spritesheet_id = (s16)spritesheet_id;
    offset = 0;
    while (slot < 9) {
        if (g_battle_gfx_spritesheet_ids_by_vram_slot[offset] == spritesheet_id) {
            return 1;
        }
        slot++;
        offset += 0x32d6;
    }
    return 0;
}
