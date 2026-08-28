#include "fft/battle.h"

void battle_unit_project_misc_to_screen(u32 misc_id, s16* screen_coords) {
    battle_screen_coords_t* screen_data;

    screen_data = battle_unit_get_screen_data_ptr_by_misc_id(misc_id);
    if (screen_data != (battle_screen_coords_t*)-1) {
        battle_camera_project_world_coords_to_screen(screen_data, screen_coords);
    }
}
