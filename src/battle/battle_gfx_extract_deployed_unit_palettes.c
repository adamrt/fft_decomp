#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_extract_deployed_unit_palettes(void) {
    s32 unused[2];

    battle_map_load_palette_data(g_battle_deployed_unit_palettes[0], 2, 1, 0);
    battle_map_load_palette_data(g_battle_deployed_unit_palettes[1], 2, 2, 0);
    battle_map_load_palette_data(g_battle_gfx_wait_direction_arrow_palette, 2, 3, 0);
    battle_map_load_palette_data(g_battle_gfx_map_selection_cursor_palette, 2, 4, 0);
    battle_map_load_palette_data(g_battle_gfx_shadow_palette, 2, 5, 0);
    battle_map_load_palette_data(g_battle_gfx_status_bubble_palette, 2, 7, 0);
    (void)&unused;
}
