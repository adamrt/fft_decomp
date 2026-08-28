/* The rotation's y halfword is loaded straight into `yaw` (one variable for
 * the raw angle and its 0xe00 part, as the target's shared $a0 shows). The
 * empty asm raises `quadrant`'s reference count so global_alloc assigns it
 * before `yaw`, giving the target's quadrant=$v1 / yaw=$a0; without it the
 * pair is swapped, and a single "r" operand is not enough.
 */
#include "fft/battle.h"
#include "psx/types.h"

/* g_battle_camera_quadrant_block_directions maps a camera quadrant index to its map-tile camera-block direction. */

void battle_camera_choose_unblocked_rotation_for_tile(
    const battle_screen_coords_t* tile_coords, battle_camera_rotation_t* rotation_out) {
    map_tile_t* tile;
    battle_camera_rotation_t* rotation;
    s32 mask;
    s32 quadrant;
    s32 yaw;
    s16 masked;

    tile = battle_map_get_tile_data_pointer(tile_coords->x, tile_coords->y, tile_coords->z);
    mask = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    rotation = (battle_camera_rotation_t*)battle_camera_get_rotation();
    rotation_out->x = rotation->x;
    rotation_out->z = rotation->z;
    yaw = rotation->y;
    masked = yaw & 0xfff;
    yaw = yaw & 0xe00;
    quadrant = masked / 0x400;
    /* Raises quadrant's use count: quadrant=$v1, yaw=$a0 (see above). */
    __asm__("" : : "r"(quadrant), "r"(quadrant));
    if ((g_battle_camera_quadrant_block_directions[quadrant] & mask) == 0
        || mask == MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK) {
        rotation_out->y = yaw;
        return;
    }
    if ((g_battle_camera_quadrant_block_directions[(quadrant + 1) & 3] & mask) == 0) {
        rotation_out->y = yaw + 0x400;
    } else if ((g_battle_camera_quadrant_block_directions[(quadrant - 1) & 3] & mask) == 0) {
        rotation_out->y = yaw - 0x400;
    } else {
        rotation_out->y = (yaw + 0x800) & 0xfff;
    }
}
