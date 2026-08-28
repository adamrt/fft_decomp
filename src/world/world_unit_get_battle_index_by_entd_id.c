#include "fft/event.h"
#include "psx/types.h"

extern s32 get_unit_id_from_misc_id(s32 misc_id);

s32 world_unit_get_battle_index_by_entd_id(s32 entd_unit_id) {
    s32 misc_id = world_get_misc_id(entd_unit_id);
    s32 unit_id;

    if (misc_id == EVENT_MISC_ID_NONE) {
        return 0x7D0;
    }
    unit_id = get_unit_id_from_misc_id(misc_id);
    if (unit_id != -1) {
        return unit_id;
    }
    unit_id = 0x7D0;
    return unit_id;
}
