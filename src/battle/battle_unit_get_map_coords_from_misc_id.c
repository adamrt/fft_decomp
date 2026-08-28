#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_get_map_coords_from_misc_id(u32 misc_id, s16* coords) {
    battle_unit_misc_data_t* unit = g_battle_unit_last_misc_data;

    if (unit != 0) {
        do {
            if (unit->unit_id == (misc_id & 0xffff)) {
                coords[0] = unit->map_x;
                coords[2] = unit->map_y;
                coords[1] = unit->map_z;
                return 1;
            }
            unit = unit->previous;
        } while (unit != 0);
    }
    main_system_handle_pointer_exception(12);
    return 0;
}
