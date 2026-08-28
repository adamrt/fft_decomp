#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

void battle_target_can_select_tile(void) {
    s32 i = 0;
    s8* dst = (s8*)g_battle_target_panel_data;
    u8* src = (u8*)g_battle_map_tile_data;
    for (; i < 0x200; i++) {
        if (src[6] & MAP_TILE_FLAG_UNTARGETABLE)
            *dst = 0;
        src += 8;
        dst += 5;
    }
}
