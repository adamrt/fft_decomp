#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_clear_horizontal_flip_flag(u32 misc_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);

    if (unit != 0) {
        unit->horizontal_flip_flag = 0;
    }
}
