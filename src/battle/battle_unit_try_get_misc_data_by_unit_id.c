#include "psx/types.h"

s32 battle_unit_try_get_misc_data_by_unit_id(u16* unit_id, s32* state) {
    u32 id;

    id = *unit_id;
    if (id != 0 && id < 0x100) {
        *unit_id = battle_get_misc_id(id);
        *state = 0;
        if (*unit_id == 0x7d0) {
            return 0;
        }
    } else {
        *state = id != 0 ? id - 0xfe : 1;
    }
    return 1;
}
