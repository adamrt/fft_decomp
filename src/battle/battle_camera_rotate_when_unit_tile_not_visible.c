#include "fft/battle.h"

void battle_camera_rotate_when_unit_tile_not_visible(battle_unit_misc_data_t* unit) {
    s32 direction;
    u8 flags;
    map_tile_t* tile;

    tile = battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    direction = (0 - g_battle_camera_render_state.vy) & 0xFFF;
    if (direction < 0) {
        direction += 0x3FF;
    }
    direction >>= 0xA;
    if (g_battle_camera_render_state.vx >= BATTLE_CAMERA_PITCH_STEEP_MASK_THRESHOLD) {
        flags = tile->camera_block_masks >> MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT;
    } else {
        flags = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    }
    switch (direction) {
    case 0:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 1:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 2:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 3:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    }
}
