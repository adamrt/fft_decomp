#include "fft/battle_unit.h"
#include "psx/types.h"

/* battle_unit_update_display for the unit in slot battle_id. No retail code calls
 * it. */
void battle_unit_update_display_by_battle_id(u16 battle_id) {
    battle_unit_update_display(battle_unit_get_by_battle_id(battle_id));
}
