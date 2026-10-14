#include "fft/battle_map.h"
#include "psx/types.h"

/* Set or blend CLUTs of a map row to `colors`: all sixteen (256 colors) when
 * update_all is 1, otherwise CLUT clut_index (16 colors). Mode 0 applies the
 * colors at once; another value starts a blend toward them and is kept as the
 * animation's mode (map_update_palette_colors_inner). Marks the rows for
 * upload. */
void map_update_palette_colors(s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;

    if (update_all == 1) {
        const palette_t* current_colors;

        index = 0;
        current_colors = colors;
        while (index < MAP_CLUTS_PER_ROW) {
            map_update_palette_colors_inner(mode, clut_row, index, *current_colors);
            index++;
            current_colors++;
        }
        g_map_palette_state.upload_pending = 1;
    } else {
        map_update_palette_colors_inner(mode, clut_row, clut_index, *colors);
        g_map_palette_state.upload_pending = 1;
    }
}
