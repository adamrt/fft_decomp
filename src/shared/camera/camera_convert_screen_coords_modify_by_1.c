#include "fft/battle_camera.h"

/* camera_convert_screen_coords_by_rotation with offset 1: the depth point of a standing unit,
 * a cursor or a bubble on a tile without MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_convert_screen_coords_modify_by_1(const SVECTOR* screen, SVECTOR* destination) {
    camera_convert_screen_coords_by_rotation(screen, destination, 1);
}
