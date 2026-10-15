#include "fft/battle_unit.h"
#include "psx/types.h"

/* Restore the facing each unit in the attacker's target list had before the
   attack. */
void battle_unit_restore_target_facings_after_attack(battle_unit_t* unit) {
    s32 i;
    battle_unit_t* target;

    if (unit->strike.target_count != 0) {
        i = 0;
        while (i < unit->strike.target_count) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
            if (target != 0) {
                battle_unit_restore_facing_after_attack(target);
            }
            i++;
        }
    }
}
