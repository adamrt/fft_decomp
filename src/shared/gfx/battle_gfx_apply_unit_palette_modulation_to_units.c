#include "fft/battle_gfx.h"

/* battle_gfx_apply_unit_palette_modulation for one unit, or for all sixteen
 * when runtime_id is 16 or more. */
void battle_gfx_apply_unit_palette_modulation_to_units(
    s32 preset_color, s32 time, s32 runtime_id, s16 red, s16 green, s16 blue) {
    s32 index;

    if (runtime_id >= BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
        for (index = 0; index < BATTLE_UNIT_RUNTIME_SLOT_COUNT; index++) {
            battle_gfx_apply_unit_palette_modulation(preset_color, time, index, red, green, blue);
        }
    } else {
        battle_gfx_apply_unit_palette_modulation(preset_color, time, runtime_id, red, green, blue);
    }
}
