#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Copies the camera rotation, snapped to the nearest yaw quadrant whose view
 * of the tile is not blocked. The camera angles are read as an SVECTOR: the
 * quadrant is a signed short division, which unsigned 16-bit values do not emit. */
void camera_choose_unblocked_rotation_for_tile(const SVECTOR* tile_coords, SVECTOR* rotation_out) {
    map_tile_t* tile;
    SVECTOR* rotation;
    s32 mask;
    s32 quadrant;
    s32 yaw;

    tile = map_get_tile_data_pointer(tile_coords->vx, tile_coords->vz, tile_coords->vy);
    mask = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    rotation = camera_get_rotation();
    rotation_out->vx = rotation->vx;
    rotation_out->vz = rotation->vz;
    quadrant = (rotation->vy & 0xfff) / 0x400;
    yaw = rotation->vy & 0xe00;
    if ((g_camera_quadrant_block_directions[quadrant] & mask) == 0 || mask == MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK) {
        rotation_out->vy = yaw;
        return;
    }
    if ((g_camera_quadrant_block_directions[(quadrant + 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw + 0x400;
    } else if ((g_camera_quadrant_block_directions[(quadrant - 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw - 0x400;
    } else {
        rotation_out->vy = (yaw + 0x800) & 0xfff;
    }
}
