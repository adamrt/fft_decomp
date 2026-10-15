#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

/* Give a unit CLUT `clut_index` of a slot's EVTCHR block, and the one eight
 * after it as its second. */
void battle_gfx_set_evtchr_palette_by_runtime_id(s32 runtime_id, s32 slot_index, s32 clut_index) {
    map_load_palette_data(&g_battle_gfx_vram_slots[slot_index].clut[clut_index], MAP_CLUT_ROW_UNITS, runtime_id, 0);
    map_load_palette_data(
        &g_battle_gfx_vram_slots[slot_index].clut[clut_index + 8], MAP_CLUT_ROW_UNITS_SECOND, runtime_id, 0);
}
