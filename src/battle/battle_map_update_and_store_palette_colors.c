#include "fft/battle.h"
#include "psx/types.h"

void battle_map_update_and_store_palette_colors(
    s32 mode, s32 palette_group, s32 palette_index, s32 update_all, const u16* colors) {
    s32 index;
    const u16* colors_reg = colors;

    battle_map_update_palette_colors(mode, palette_group, palette_index, update_all, colors_reg);

    if (update_all == 1) {
        u16* destination;
        const u16* source;

        index = 0;
        destination = g_battle_map_palette_state.banks[palette_group].packed_colors;
        source = colors_reg;
        while (index < 256) {
            *destination++ = *source++;
            index++;
        }
    } else {
        const u16* source;
        u16* palette_colors;
        s32 palette_offset;

        index = 0;
        palette_offset = palette_index * 16;
        palette_colors = g_battle_map_palette_state.banks[palette_group].packed_colors;
        source = colors_reg;
        while (index < 16) {
            palette_colors[palette_offset + index] = *source++;
            index++;
        }
    }
}
