#include "fft/battle_camera.h"

/* camera_calculate_relative_offset with offset 5: the depth point of a stepping unit on a
 * tile with MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_calculate_relative_offset_by_5(const SVECTOR* input, SVECTOR* output) {
    camera_calculate_relative_offset(input, output, 5);
}
