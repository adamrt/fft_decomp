#include "psx/types.h"

void battle_gfx_tint_all_units_by_team(void) {
    s32 i = 0;
    do {
        battle_gfx_modify_palette_based_on_team(battle_unit_get_misc_data_by_misc_id((u16)i), 4);
        i++;
    } while (i < 0x10);
}
