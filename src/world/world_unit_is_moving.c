#include "fft/unit_slots.h"
#include "psx/types.h"

/* Return whether one unit, or any existing unit when id is -1, is moving. */
s32 world_unit_is_moving(s32 id) {
    s32 i;

    if (id != -1) {
        if (battle_move_is_unit_moving_by_misc_id(id) != 0) {
            return 1;
        }
    } else {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i += 1) {
            if (battle_unit_has_misc_id(i) != 0 && battle_move_is_unit_moving_by_misc_id(i) != 0) {
                return 1;
            }
        }
    }
    return 0;
}
