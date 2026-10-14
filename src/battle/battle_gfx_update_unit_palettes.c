#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Ends the repeating tint of every unit CLUT that has an animation running and
 * fades it back to its last applied colors. */
void battle_gfx_update_unit_palettes(void) {
    s32 palette_index;

    for (palette_index = 0; palette_index < 0x10; palette_index++) {
        if (g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNITS].animations[palette_index].active != 0) {
            map_modify_palette(MAP_COLOR_MODE_STOP_REPEAT, 4, MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
            map_modify_palette(MAP_COLOR_MODE_RESTORE, 4, MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
        }
    }
}
