#include "fft/effect.h"

enum {
    MISC_UNIT_SLOT_COUNT = 16,
};

void battle_gfx_start_misc_unit_palette_modulation(
    s32 preset_color, s32 time, s32 misc_id, s32 red, s32 green, s32 blue) {
    s32 index;

    if (misc_id >= MISC_UNIT_SLOT_COUNT) {
        for (index = 0; index < MISC_UNIT_SLOT_COUNT; index++) {
            battle_gfx_apply_misc_unit_palette_modulation(preset_color, time, index, (s16)red, (s16)green, (s16)blue);
        }
    } else {
        battle_gfx_apply_misc_unit_palette_modulation(preset_color, time, misc_id, (s16)red, (s16)green, (s16)blue);
    }
}
