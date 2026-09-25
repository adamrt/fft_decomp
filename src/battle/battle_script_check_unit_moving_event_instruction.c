#include "fft/battle.h"
#include "psx/types.h"

/* Event instruction 0x39: is the unit with this misc id moving (any of the
 * 21 units when misc id is -1)? */
s32 battle_script_check_unit_moving_event_instruction(s32 misc_id) {
    s32 i;

    if (misc_id != -1) {
        if (battle_move_is_unit_moving_by_misc_id(misc_id) != 0) {
            return 1;
        }
    } else {
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if ((battle_unit_has_misc_id(i) != 0) && (battle_move_is_unit_moving_by_misc_id(i) != 0)) {
                return 1;
            }
        }
    }
    return 0;
}
