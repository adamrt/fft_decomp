#include "fft/battle.h"

void battle_unit_copy_misc_screen_location(u32 misc_id, VECTOR* destination) {
    battle_screen_coords_t* screen_data;

    screen_data = battle_unit_get_screen_data_ptr_by_misc_id(misc_id);
    destination->vx = screen_data->x;
    destination->vy = screen_data->z;
    destination->vz = screen_data->y;
}
