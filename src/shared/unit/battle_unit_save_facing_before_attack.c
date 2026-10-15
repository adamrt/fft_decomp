#include "fft/battle_unit.h"
#include "psx/types.h"

/* If the unit has no attack facing yet (0xffff), seed it from the unit's
 * current facing so the attack animation turns towards the same direction.
 * The target reads attack_facing with a signed `lh`, hence the s16 access. */
void battle_unit_save_facing_before_attack(battle_unit_t* unit) {
    s16 facing;
    char unused[16];

    if (unit != 0) {
        if (unit->attack_facing == -1) {
            facing = unit->facing;
            unit->attack_facing = facing;
        }
    }
}
