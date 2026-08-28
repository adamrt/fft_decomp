#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

void battle_map_store_selected_tile_coordinates(void* destination) {
    main_util_set_svector((SVECTOR*)destination, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
}
