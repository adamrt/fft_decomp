#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

void battle_target_clear_panels_on_untargetable_tiles(void) {
    s32 i;
    u8* src;
    s8* dst;

    i = 0;
    dst = (s8*)g_battle_target_panel_data;
    src = (u8*)g_battle_map_tile_data;
    do {
        if (src[6] & MAP_TILE_FLAG_UNTARGETABLE) {
            dst[0] = 0;
            dst[1] = 0;
        }
        src += 8;
        i += 1;
        dst += 5;
    } while (i < 0x100);
}
