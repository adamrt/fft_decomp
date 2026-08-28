#include "fft/battle_camera.h"

/* Copy a Misc Unit's renderer-space coordinates into the shared coordinate
 * triple. A -1 lookup result leaves the destination unchanged.
 */
battle_screen_coords_t* battle_copy_misc_unit_screen_coords(u32 misc_id) {
    battle_screen_coords_t* source;
    battle_screen_coords_t* dest;

    source = battle_unit_get_screen_data_ptr_by_misc_id(misc_id);

    if (source != (battle_screen_coords_t*)-1) {
        dest = &g_battle_current_misc_screen_coords;
        dest->x = source->x;
        g_battle_current_misc_screen_coords.z = source->z;
        g_battle_current_misc_screen_coords.y = source->y;
        return dest;
    }
    return (battle_screen_coords_t*)-1;
}
