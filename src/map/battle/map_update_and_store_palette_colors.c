#include "fft/battle_map.h"
#include "psx/types.h"

/* map_update_palette_colors, then copy the colors into the row's packed
 * mirror as well, which makes them the base colors that the MAP_COLOR_MODE_BASE
 * modes and RESTORE of map_modify_palette start from: all 256 when
 * update_all is 1, otherwise the 16 of CLUT clut_index. */
void map_update_and_store_palette_colors(
    s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;
    const palette_t* colors_reg = colors;

    map_update_palette_colors(mode, clut_row, clut_index, update_all, colors_reg);

    /* The packed mirror is copied with native 16-bit loads and stores. */
    if (update_all == 1) {
        u16* destination;
        const u16* source;

        index = 0;
        destination = g_map_palette_state.clut_rows[clut_row].packed_colors;
        source = (const u16*)colors_reg;
        while (index < MAP_CLUT_ROW_COLOR_COUNT) {
            *destination++ = *source++;
            index++;
        }
    } else {
        const u16* source;
        u16* palette_colors;
        s32 palette_offset;

        index = 0;
        palette_offset = clut_index * MAP_CLUT_COLOR_COUNT;
        palette_colors = g_map_palette_state.clut_rows[clut_row].packed_colors;
        source = (const u16*)colors_reg;
        while (index < MAP_CLUT_COLOR_COUNT) {
            palette_colors[palette_offset + index] = *source++;
            index++;
        }
    }
}
