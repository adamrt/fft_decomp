#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Remove the range from every panel, on both levels, whose tile is flagged
 * untargetable. */
void target_clear_range_on_untargetable_tiles(void) {
    s32 i = 0;
    target_panel_t* dst = g_target_panel_data;
    map_tile_t* src = g_map_tile_data;
    for (; i < 0x200; i++) {
        if (src->flags.untargetable)
            dst->remaining_range = 0;
        src++;
        dst++;
    }
}
