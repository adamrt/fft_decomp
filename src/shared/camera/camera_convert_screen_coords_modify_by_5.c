#include "fft/battle_camera.h"

/* camera_convert_screen_coords_by_rotation with offset 5: the depth point on a tile with
 * MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_convert_screen_coords_modify_by_5(const SVECTOR* screen, SVECTOR* destination) {
    camera_convert_screen_coords_by_rotation(screen, destination, 5);
}
