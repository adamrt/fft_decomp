#include "fft/battle.h"
#include "fft/world.h"

void world_unit_copy_screen_data_by_misc_id(s32 misc_id, s16* out) {
    battle_screen_coords_t* screen_data = battle_unit_get_screen_data_ptr_by_misc_id(misc_id);

    if (screen_data != (battle_screen_coords_t*)-1) {
        out[0] = screen_data->x;
        out[1] = screen_data->z;
        out[2] = screen_data->y;
    }
}
