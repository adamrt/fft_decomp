#include "fft/battle.h"

void battle_unit_copy_misc_screen_coords(u32 misc_id, u16* screen_coords) {
    battle_screen_coords_t* screen_data;

    screen_data = battle_unit_get_screen_data_ptr_by_misc_id(misc_id);
    if (screen_data != (battle_screen_coords_t*)-1) {
        screen_coords[0] = screen_data->x;
        screen_coords[1] = screen_data->z;
        screen_coords[2] = screen_data->y;
    }
}
