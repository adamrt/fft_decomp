#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_update_and_animate_units(void) {
    battle_unit_misc_data_t* unit;

    unit = g_battle_unit_misc_list_head;
    while (unit != 0) {
        battle_gfx_update_and_animate_unit_wep_eff(unit);
        battle_gfx_init_status_bubble(unit);
        battle_unit_update_fade_out_removal(unit);
        unit = unit->previous;
    }
}
