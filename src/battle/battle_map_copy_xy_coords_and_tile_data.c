#include "fft/battle.h"
#include "psx/libc.h"
#include "psx/types.h"

void battle_map_copy_xy_coords_and_tile_data(u8* data) {
    g_battle_map_max_x = *data++;
    g_battle_map_max_y = *data++;
    memcpy((u8*)g_battle_map_tile_data, data, 0x1000);
}
