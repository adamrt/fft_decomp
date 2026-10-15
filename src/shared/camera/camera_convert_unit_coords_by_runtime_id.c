#include "fft/battle_camera.h"

/* camera_convert_screen_coords_modify_by_1 on a unit's position. An absent
 * unit passes address 0 on. No retail code calls it. */
void camera_convert_unit_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    camera_convert_screen_coords_modify_by_1(battle_unit_get_world_coords_by_runtime_id(runtime_id & 0xffff), coords);
}
