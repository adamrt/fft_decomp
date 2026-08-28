#include "fft/battle.h"
#include "psx/types.h"

void battle_map_update_palette_colors(
    s32 mode, s32 palette_group, s32 palette_index, s32 update_all, const u16* colors) {
    s32 index;

    if (update_all == 1) {
        const u16* current_colors;

        index = 0;
        current_colors = colors;
        while (index < 16) {
            battle_map_update_palette_colors_inner(mode, palette_group, index, current_colors);
            index++;
            current_colors += 16;
        }
        g_battle_map_palette_state.upload_pending = 1;
    } else {
        battle_map_update_palette_colors_inner(mode, palette_group, palette_index, colors);
        g_battle_map_palette_state.upload_pending = 1;
    }
}
