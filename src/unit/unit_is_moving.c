#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Test one runtime ID; -1 scans all 21 event slots. */
s32 unit_is_moving(s32 runtime_id) {
    s32 i;

    if (runtime_id != -1) {
        if (move_is_unit_moving_by_runtime_id(runtime_id) != 0) {
            return 1;
        }
    } else {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            if ((battle_unit_has_runtime_id(i) != 0) && (move_is_unit_moving_by_runtime_id(i) != 0)) {
                return 1;
            }
        }
    }
    return 0;
}
