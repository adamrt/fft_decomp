#include "fft/battle.h"

s32 battle_unit_remove_misc_by_id(u32 misc_id) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xFFFF);
    if (unit != 0) {
        battle_unit_remove_misc(unit);
        return 1;
    }
    main_system_handle_pointer_exception(0xC);
    return 0;
}
