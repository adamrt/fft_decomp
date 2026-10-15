#include "fft/battle_gfx.h"
#include "psx/types.h"

void battle_gfx_tint_all_units_by_team(void) {
    s32 i;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        battle_gfx_modify_palette_based_on_team(battle_unit_get_by_runtime_id(i), 4);
    }
}
