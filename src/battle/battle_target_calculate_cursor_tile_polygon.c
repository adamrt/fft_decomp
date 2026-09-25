#include "fft/battle.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Build the four corner vertices of the map tile under a cursor position.
 *
 * The corners sit 14 units either side of coords on X and Y. Their height is
 * the tile's surface (height plus depth, 12 units per step) raised by the slope
 * half-height, then each slope type raises or lowers individual corners by that
 * half-height. Returns how the quad is triangulated: 0 planar, 1 split along
 * corners 0-3, 2 split along corners 1-2.
 *
 * split is cleared before the tile lookup: living across the call keeps it in
 * a saved register, as in the target, although the scheduler emits the clear
 * after the call. Testing depth & MAP_TILE_HALF_HEIGHT_MASK directly (rather
 * than masking depth in place) keeps the height load ahead of the depth load.
 */
s32 battle_target_calculate_cursor_tile_polygon(battle_screen_coords_t* coords, u8 layer, SVECTOR* quad) {
    map_tile_t* tile;
    s32 split;
    s32 rise;
    u8 height;
    u8 depth;
    u8 slope;

    split = 0;
    tile = battle_map_get_tile_data_pointer((s16)(coords->x / 28), (s16)(coords->y / 28), layer);
    height = tile->height;
    depth = tile->depth_half_height;
    slope = tile->slope_type;
    quad[0].vx = coords->x - 14;
    quad[0].vz = coords->y + 14;
    quad[1].vx = coords->x + 14;
    quad[1].vz = coords->y + 14;
    quad[2].vx = coords->x - 14;
    quad[2].vz = coords->y - 14;
    quad[3].vx = coords->x + 14;
    quad[3].vz = coords->y - 14;
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -((height + (depth >> MAP_TILE_DEPTH_SHIFT)) * 12);
    if ((depth & MAP_TILE_HALF_HEIGHT_MASK) != 0) {
        rise = (depth & MAP_TILE_HALF_HEIGHT_MASK) * 6;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
        switch (slope) {
        case 0x52: /* edge up to east */
            quad[0].vy = quad[2].vy = quad[2].vy + rise;
            quad[1].vy = quad[3].vy = quad[3].vy - rise;
            break;
        case 0x58: /* edge up to west */
            quad[1].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[2].vy = quad[2].vy - rise;
            break;
        case 0x25: /* edge up to south */
            quad[0].vy = quad[1].vy = quad[1].vy + rise;
            quad[2].vy = quad[3].vy = quad[3].vy - rise;
            break;
        case 0x85: /* edge up to north */
            quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[1].vy = quad[1].vy - rise;
            break;
        case 0x41: /* low corner, north-east up */
            split = 1;
            quad[0].vy = quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[1].vy = quad[1].vy - rise;
            break;
        case 0x11: /* low corner, south-east up */
            split = 2;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[2].vy + rise;
            quad[3].vy = quad[3].vy - rise;
            break;
        case 0x14: /* low corner, south-west up */
            split = 1;
            quad[0].vy = quad[1].vy = quad[3].vy = quad[3].vy + rise;
            quad[2].vy = quad[2].vy - rise;
            break;
        case 0x44: /* low corner, north-west up */
            split = 2;
            quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[0].vy - rise;
            break;
        case 0x96: /* high corner, north-east up */
            split = 1;
            quad[0].vy = quad[1].vy = quad[3].vy = quad[3].vy - rise;
            quad[2].vy = quad[2].vy + rise;
            break;
        case 0x66: /* high corner, south-east up */
            split = 2;
            quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
            quad[0].vy = quad[0].vy + rise;
            break;
        case 0x69: /* high corner, south-west up */
            split = 1;
            quad[0].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
            quad[1].vy = quad[1].vy + rise;
            break;
        case 0x99: /* high corner, north-west up */
            split = 2;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[2].vy - rise;
            quad[3].vy = quad[3].vy + rise;
            break;
        }
    }
    return split;
}
