#include "fft/battle.h"
#include "psx/types.h"

/* If the unit has no attack facing yet (0xffff), seed it from the unit's
 * current facing so the attack animation turns towards the same direction.
 * The target reads attack_facing with a signed `lh`, hence the s16 access. */
void battle_unit_save_facing_before_attack(battle_unit_misc_data_t* unit) {
    s16 facing;
    char unused[16];

    if (unit != 0) {
        if (*(s16*)&unit->attack_facing == -1) {
            facing = unit->facing;
            unit->attack_facing = facing;
        }
    }
}
