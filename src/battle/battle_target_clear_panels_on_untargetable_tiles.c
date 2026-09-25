#include "fft/battle.h"
#include "psx/types.h"

void battle_target_clear_panels_on_untargetable_tiles(void) {
    s32 i;
    map_tile_t* src;
    targeting_panel_entry_t* dst;

    i = 0;
    dst = g_battle_target_panel_data;
    src = g_battle_map_tile_data;
    do {
        if (src->flags_06.value & MAP_TILE_FLAG_UNTARGETABLE) {
            dst->a = 0;
            dst->b = 0;
        }
        src++;
        i += 1;
        dst++;
    } while (i < 0x100);
}
