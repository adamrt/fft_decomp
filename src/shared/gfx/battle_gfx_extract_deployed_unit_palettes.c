#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

/* Uploads the fixed CLUTs of the sprite row: 1 and 2 the deployed-unit palettes,
 * 3 the wait-direction arrow, 4 the map selection cursor, 5 the shadow and 7 the
 * status bubble. */
void battle_gfx_extract_deployed_unit_palettes(void) {
    RECT rect; /* never used; retail reserves its 8 bytes */

    map_load_palette_data(&g_battle_deployed_unit_palettes[0], MAP_CLUT_ROW_SPRITES, 1, 0);
    map_load_palette_data(&g_battle_deployed_unit_palettes[1], MAP_CLUT_ROW_SPRITES, 2, 0);
    map_load_palette_data(&g_battle_gfx_wait_direction_arrow_palette, MAP_CLUT_ROW_SPRITES, 3, 0);
    map_load_palette_data(&g_battle_gfx_map_selection_cursor_palette, MAP_CLUT_ROW_SPRITES, 4, 0);
    map_load_palette_data(&g_battle_gfx_shadow_palette, MAP_CLUT_ROW_SPRITES, 5, 0);
    map_load_palette_data(&g_battle_gfx_status_bubble_palette, MAP_CLUT_ROW_SPRITES, 7, 0);
}
