#include "fft/battle.h"
#include "psx/types.h"

void battle_map_update_palette_colors_by_index(s32 map_slot, s32 palette_index, const void* color_data) {
    s32 index;

    if (palette_index >= 16) {
        index = 0;
        do {
            battle_map_update_palette_colors(map_slot, 3, index, 0, (const u16*)color_data);
            index++;
        } while (index < 16);
    } else {
        battle_map_update_palette_colors(map_slot, 3, palette_index, 0, (const u16*)color_data);
    }
}
