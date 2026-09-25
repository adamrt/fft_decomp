#include "fft/battle.h"
#include "fft/world.h"

void world_unit_project_misc_to_screen(s32 misc_id, s16* screen_coords) {
    battle_screen_coords_t* world_coords = battle_unit_get_screen_data_ptr_by_misc_id(misc_id);

    if (world_coords != (battle_screen_coords_t*)-1) {
        battle_camera_project_world_coords_to_screen(world_coords, screen_coords);
    }
}
