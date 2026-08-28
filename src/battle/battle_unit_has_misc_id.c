#include "psx/types.h"

s32 battle_unit_has_misc_id(u32 misc_id) {
    return battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff) != 0;
}
