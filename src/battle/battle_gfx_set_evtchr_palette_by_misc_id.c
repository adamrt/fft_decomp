#include "fft/battle_gfx.h"
#include "fft/map.h"

void battle_gfx_set_evtchr_palette_by_misc_id(s32 palette_row_index, s32 slot_index, s32 clut_index) {
    battle_map_load_palette_data(g_battle_gfx_vram_slots[slot_index].cluts[clut_index], 3, palette_row_index, 0);
    battle_map_load_palette_data(g_battle_gfx_vram_slots[slot_index].cluts[clut_index + 8], 10, palette_row_index, 0);
}
