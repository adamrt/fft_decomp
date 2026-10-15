#include "fft/battle_map.h"
#include "psx/types.h"

/* map_update_palette_colors for all sixteen CLUTs of the map row. */
void map_update_all_palettes_from_color_data(s32 mode, const clut_t color_data) {
    map_update_palette_colors(mode, MAP_CLUT_ROW_MAP, 0, 1, color_data);
}
