#include "fft/battle.h"

battle_screen_coords_t* battle_unit_get_screen_data_ptr_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return (battle_screen_coords_t*)&unit->screen;
}
