#include "fft/battle_map.h"
#include "psx/types.h"

/* Load colors into a map CLUT row and mark the rows for upload: the row's
 * sixteen CLUTs (256 colors) when load_all is 1, otherwise the sixteen colors
 * of CLUT clut_index. The blends of the loaded CLUTs stop. Nothing happens while
 * the row's updates are disabled. */
void map_load_palette_data(const palette_t* palettes, s32 clut_row, s32 clut_index, s32 load_all) {
    s32 clear_index;
    s32 i;

    if (g_map_palette_state.clut_rows[clut_row].updates_disabled != 0) {
        return;
    }

    if (load_all == 1) {
        for (clear_index = MAP_CLUTS_PER_ROW - 1; clear_index >= 0; clear_index--) {
            g_map_palette_state.clut_rows[clut_row].animations[clear_index].active = 0;
        }

        for (i = 0; i < MAP_CLUTS_PER_ROW; i++) {
            map_extract_palette_colors(palettes[i], clut_row, i);
        }
    } else {
        g_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
        map_extract_palette_colors(*palettes, clut_row, clut_index);
    }

    g_map_palette_state.upload_pending = 1;
}
