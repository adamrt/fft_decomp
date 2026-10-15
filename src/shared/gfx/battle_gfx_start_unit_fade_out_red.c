#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Start a unit's fade-out removal with a red flash: fade step 4 and a red offset
 * of 0x1f. */
void battle_gfx_start_unit_fade_out_red(s32 runtime_id) {
    battle_gfx_start_unit_fade_out(runtime_id, 4, 0x1F, 0, 0);
}
