#include "fft/battle.h"

/* Formula 0x64: damage (PA * WP); spear (PA * 3/2 * WP); weaponless (PA * Brave/100 * WP). */
void battle_formula_damage_pa_times_wp(void) {
    battle_formula_init_weapon_xa_ya();
    battle_formula_apply_physical_status_support_compatibility();
    battle_formula_store_xa_times_ya_damage();
}
