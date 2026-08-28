#include "fft/battle.h"

void battle_target_move_cursor_to_unit(battle_unit_misc_data_t* unit) {
    if (unit != 0) {
        g_battle_cursor_x = unit->map_x;
        g_battle_cursor_y = unit->map_y;
        g_battle_cursor_z = unit->map_z;
    }
}
