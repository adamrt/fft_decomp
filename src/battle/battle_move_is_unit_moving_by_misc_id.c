#include "fft/battle.h"
#include "psx/types.h"

s32 battle_move_is_unit_moving_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);

    if (unit == 0) {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return unit->movement_path_count != 0;
}
