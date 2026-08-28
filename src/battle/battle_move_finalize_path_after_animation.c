#include "fft/battle_move.h"
#include "psx/types.h"

void battle_move_finalize_path_after_animation(battle_unit_misc_data_t* unit) {
    u8 path_count;

    if (unit->animation_countdown == 0) {
        path_count = unit->movement_path_count;
        unit->centre_tile_offset = 0;
        unit->movement_path_count = 0;
        unit->last_path_count = path_count;
    }
}
