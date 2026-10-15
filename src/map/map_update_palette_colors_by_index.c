#include "fft/battle_map.h"
#include "psx/types.h"

/* map_update_palette_colors for one CLUT of the unit row. A clut_index of
 * 16 or more applies the same sixteen colors to every unit CLUT. */
void map_update_palette_colors_by_index(s32 mode, s32 clut_index, const palette_t* color_data) {
    s32 index;

    if (clut_index >= MAP_CLUTS_PER_ROW) {
        for (index = 0; index < MAP_CLUTS_PER_ROW; index++) {
            map_update_palette_colors(mode, MAP_CLUT_ROW_UNITS, index, 0, color_data);
        }
    } else {
        map_update_palette_colors(mode, MAP_CLUT_ROW_UNITS, clut_index, 0, color_data);
    }
}
