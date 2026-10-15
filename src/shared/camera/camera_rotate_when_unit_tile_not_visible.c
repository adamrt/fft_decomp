#include "fft/battle_camera.h"
#include "fft/battle_map.h"

/* Turns the camera when the unit's tile is blocked from the current view.
 *
 * The view quadrant is (-yaw & 0xfff) / 0x400; quadrants 0..3 test the tile's
 * north-west, south-west, south-east and north-east camera-block bits. The
 * steep-pitch nibble of camera_block_masks is used at a pitch of
 * CAMERA_PITCH_STEEP_MASK_THRESHOLD or more, the flat-pitch nibble below
 * it. When the current quadrant's bit is set, the first unblocked of a quarter
 * turn (0x400), the opposite quarter turn (-0x400) and a half turn (0x800) is
 * requested; with all four bits set the camera stays.
 *
 * Called when the acting unit starts a cast, when the target-display state shows
 * the casting unit's auto-battle target and when the action menus open for a
 * scripted turn. The `direction < 0` adjustment cannot run: the value is already
 * masked to 0..0xfff. */
void camera_rotate_when_unit_tile_not_visible(const battle_unit_t* unit) {
    s32 direction;
    u8 flags;
    map_tile_t* tile;

    tile = map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    direction = (0 - g_camera_render_state.vy) & 0xFFF;
    if (direction < 0) {
        direction += 0x3FF;
    }
    direction >>= 0xA;
    if (g_camera_render_state.vx >= CAMERA_PITCH_STEEP_MASK_THRESHOLD) {
        flags = tile->camera_block_masks >> MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT;
    } else {
        flags = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    }
    switch (direction) {
    case 0:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 1:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 2:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 3:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    }
}
