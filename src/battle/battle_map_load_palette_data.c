#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

void battle_map_load_palette_data(const u16* palette, s32 mesh_index, s32 palette_row_index, s32 load_all_rows) {
    s32 clear_index;
    s32 row_index;
    const u16* palette_row;

    if (g_battle_map_palette_state.banks[mesh_index].updates_disabled != 0) {
        return;
    }

    if (load_all_rows == 1) {
        clear_index = MAP_PALETTE_ROW_COUNT - 1;
        do {
            g_battle_map_palette_state.banks[mesh_index].animations[clear_index].active = 0;
            clear_index--;
        } while (clear_index >= 0);

        row_index = 0;
        palette_row = palette;
        do {
            battle_map_extract_palette_colors(palette_row, mesh_index, row_index);
            row_index++;
            palette_row += MAP_PALETTE_COLORS_PER_ROW;
        } while (row_index < MAP_PALETTE_ROW_COUNT);
    } else {
        g_battle_map_palette_state.banks[mesh_index].animations[palette_row_index].active = 0;
        battle_map_extract_palette_colors(palette, mesh_index, palette_row_index);
    }

    g_battle_map_palette_state.upload_pending = 1;
}
