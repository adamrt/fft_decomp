#include "fft/battle_gfx.h"

/* The sprite height of the unit's sheet, or 0 for an absent, dead, crystal or
 * treasure unit. */
u32 battle_gfx_get_unit_spritesheet_height(battle_unit_t* unit) {
    if (unit == 0) {
        return 0;
    }
    if ((unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE)) != 0) {
        return 0;
    }
    if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) != 0) {
        return 0;
    }
    return g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height;
}
