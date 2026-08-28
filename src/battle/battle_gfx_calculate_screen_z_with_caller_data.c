#include "fft/battle.h"

s32 battle_gfx_calculate_screen_z_with_caller_data(
    battle_unit_misc_data_t* unit, const battle_screen_coords_t* coords) {
    battle_unit_misc_data_t* mount;
    s32 adjusted_screen_z;
    s32 screen_z;

    mount = battle_unit_get_mount_misc_data(unit);
    screen_z = battle_calculate_screen_z_from_input_coords(mount, coords);
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        adjusted_screen_z = screen_z - 10;
        screen_z = mount->mounted_height_offset + adjusted_screen_z;
    }
    return (s16)screen_z;
}
