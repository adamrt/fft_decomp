#include "fft/battle.h"

void battle_unit_set_move_and_screen_coords(battle_unit_misc_data_t* unit) {
    unit->movement.bytes.destination_x = unit->map_x;
    unit->movement.bytes.destination_y = unit->map_y;
    unit->movement.bytes.destination_z = unit->map_z;
    unit->screen.vx = unit->map_x * 28 + 14;
    unit->screen.vz = unit->map_y * 28 + 14;
    unit->screen.vy = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
    battle_unit_set_real_coords_from_screen_coords(unit);
}
