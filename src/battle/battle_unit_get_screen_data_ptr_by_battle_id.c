#include "fft/battle.h"

void* battle_unit_get_screen_data_ptr_by_battle_id(u32 battle_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->screen.vx;
}
