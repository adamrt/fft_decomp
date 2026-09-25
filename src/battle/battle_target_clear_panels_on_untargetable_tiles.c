#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

void battle_target_clear_panels_on_untargetable_tiles(void) {
    s32 i;
    u8* src;
    targeting_panel_entry_t* dst;

    i = 0;
    dst = g_battle_target_panel_data;
    src = (u8*)g_battle_map_tile_data;
    do {
        if (src[6] & MAP_TILE_FLAG_UNTARGETABLE) {
            dst->a = 0;
            dst->b = 0;
        }
        src += 8;
        i += 1;
        dst++;
    } while (i < 0x100);
}
