#include "fft/battle.h"
#include "fft/world.h"

void world_unit_copy_screen_coordinates(u32 misc_id, VECTOR* destination) {
    battle_screen_coords_t* screen_data;

    screen_data = battle_unit_get_screen_data_ptr_by_misc_id(misc_id);
    destination->vx = screen_data->x;
    destination->vy = screen_data->z;
    destination->vz = screen_data->y;
}
