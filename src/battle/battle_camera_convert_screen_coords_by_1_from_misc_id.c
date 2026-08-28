#include "fft/battle.h"

void battle_camera_convert_screen_coords_by_1_from_misc_id(u32 misc_id, s16* coords) {
    battle_camera_convert_screen_coords_modify_by_1(
        (const s16*)battle_unit_get_screen_data_ptr_by_misc_id(misc_id & 0xffff), coords);
}
