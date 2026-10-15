#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Clear the range and the mark of each panel whose tile is flagged untargetable.
 * Only the 256 lower-level panels are visited. */
void target_clear_panels_on_untargetable_tiles(void) {
    s32 i;
    map_tile_t* src;
    target_panel_t* dst;

    i = 0;
    dst = g_target_panel_data;
    src = g_map_tile_data;
    for (; i < 256; src++, i++, dst++) {
        if (src->flags.untargetable) {
            dst->remaining_range = 0;
            dst->mark = 0;
        }
    }
}
