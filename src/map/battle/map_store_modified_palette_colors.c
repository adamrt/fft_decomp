#include "fft/battle_map.h"
#include "psx/types.h"

void map_store_modified_palette_colors(s32 clut_row, s32 clut_index, s32 store_all) {
    if (store_all == 1) {
        u16* destination;
        u16* source;

        store_all = 0;
        destination = g_map_palette_state.clut_rows[clut_row].packed_colors;
        source = g_map_palette_modified_colors[0];
        source += clut_row * MAP_CLUT_ROW_COLOR_COUNT;
        while (store_all < MAP_CLUT_ROW_COLOR_COUNT) {
            *destination++ = *source++;
            store_all++;
        }
    } else {
        u16* packed_row;

        store_all = 0;
        clut_index *= MAP_CLUT_COLOR_COUNT;
        packed_row = g_map_palette_state.clut_rows[clut_row].packed_colors;
        while (store_all < MAP_CLUT_COLOR_COUNT) {
            packed_row[clut_index + store_all] = g_map_palette_modified_colors[clut_row][clut_index + store_all];
            store_all++;
        }
    }
}
