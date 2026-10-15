#include "fft/battle.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Selector 15 of map_set_tile_data_value retries this until it returns 0, which it does at
 * once. The 8-byte frame is one the retail build kept for an unreferenced local. */
s32 map_tile_data_15_stub(void) {
    char unused[8];

    return 0;
}
