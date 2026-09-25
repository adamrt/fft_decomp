#include "fft/battle.h"
#include "psx/types.h"

/* Configure mode-4 palette modulation with a red offset of 0x1f. */
void battle_gfx_configure_misc_unit_palette_modulation_1f(s32 misc_id) {
    battle_gfx_configure_misc_unit_palette_modulation(misc_id, 4, 0x1F, 0, 0);
}
