#include "fft/map.h"
#include "psx/types.h"

void battle_map_update_all_palettes_from_color_data(s32 map_slot, const void* color_data) {
    battle_map_update_palette_colors(map_slot, 0, 0, 1, (const u16*)color_data);
}
