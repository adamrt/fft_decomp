#include "fft/battle_unit.h"
#include "psx/types.h"

/* battle_unit_restore_facing_after_attack for every runtime id 0..15. */
void battle_unit_restore_all_facings_after_attack(void) {
    s32 i;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        battle_unit_t* unit = battle_unit_get_by_runtime_id(i & 0xFFFF);
        if (unit) {
            battle_unit_restore_facing_after_attack(unit);
        }
    }
}
