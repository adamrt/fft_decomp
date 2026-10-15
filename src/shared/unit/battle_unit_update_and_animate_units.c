#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Per-frame update of every unit record: its animation with the weapon and effect
 * sprites, its status bubble and its fade-out removal. */
void battle_unit_update_and_animate_units(void) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_gfx_update_and_animate_unit_wep_eff(unit);
        battle_gfx_init_status_bubble(unit);
        battle_unit_update_fade_out_removal(unit);
        unit = unit->previous;
    }
}
