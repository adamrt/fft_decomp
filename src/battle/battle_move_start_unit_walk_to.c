#include "fft/battle_move.h"
#include "psx/types.h"

/* Path a unit to a destination tile and start it walking at the given speed. */
s32 battle_move_start_unit_walk_to(s32 misc_id, s16* destination, s32 flags, s32 jump, s32 speed) {
    battle_unit_misc_data_t* unit;
    battle_walk_path_t* path;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xFFFF);
    if (unit != 0) {
        path = battle_move_calculate_walkto_pathing(
            flags, jump, unit->map_x, unit->map_y, unit->map_z, destination[0], destination[2], destination[1]);
        if (path != 0) {
            *(battle_walk_path_t*)&unit->movement_path_count = *path;
            unit->walk_speed.word = speed * 2;
            unit->step_speed = 0x2000;
            unit->movement_path_offset = 0;
            battle_unit_dismount_rider_and_update_display(unit);
            return 1;
        }
    } else {
        main_system_handle_pointer_exception(0xC);
    }
    return 0;
}
