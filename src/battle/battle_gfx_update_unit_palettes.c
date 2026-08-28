#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_update_unit_palettes(void) {
    s32 palette_index = 0;
    s32 flag_offset = 0;

    while (palette_index < 0x10) {
        if (((u8*)g_battle_gfx_palette_update_flags)[flag_offset] != 0) {
            battle_map_modify_palette(10, 4, 3, palette_index, 0, 0, 0, 0);
            battle_map_modify_palette(8, 4, 3, palette_index, 0, 0, 0, 0);
        }
        palette_index++;
        flag_offset += 8;
    }
}
