#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_update_and_animate_units(void) {
    u8* unit;

    unit = (u8*)g_battle_unit_misc_list_head;
    while (unit != 0) {
        battle_gfx_update_and_animate_unit_wep_eff(unit);
        battle_gfx_init_status_bubble((battle_unit_misc_data_t*)unit);
        battle_unit_update_fade_out_removal((battle_unit_misc_data_t*)unit);
        unit = *(u8**)unit;
    }
}
