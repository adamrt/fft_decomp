#include "fft/event.h"
#include "psx/types.h"

s32 battle_unit_get_battle_index_by_entd_unit_id(s32 unit_id) {
    s32 misc_id = battle_get_misc_id(unit_id);
    if (misc_id != EVENT_MISC_ID_NONE) {
        s32 idx = battle_unit_get_battle_index_by_misc_id(misc_id);
        if (idx != -1) {
            return idx;
        }
    }
    return 0x7D0;
}
