#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_init_misc_unit_palette_modulation(s32 misc_id, s32 mode, s16 red, s16 green, s16 blue) {
    battle_gfx_configure_misc_unit_palette_modulation(misc_id, mode, red, green, blue);
}
