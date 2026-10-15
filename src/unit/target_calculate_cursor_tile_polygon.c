#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/libgte.h"
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
s32 target_calculate_cursor_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad) {
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    s32 split;
    s32 rise;
    u8 height;
    u8 depth;
    u8 slope;

    split = 0;
    tile = map_get_tile_data_pointer((coords->vx / MAP_TILE_SIZE), (coords->vz / MAP_TILE_SIZE), layer);
    height = tile->height;
    depth = (*(u8*)&tile->depth_half_height);
    slope = tile->slope_type;
    quad[0].vx = coords->vx - 14;
    quad[0].vz = coords->vz + 14;
    quad[1].vx = coords->vx + 14;
    quad[1].vz = coords->vz + 14;
    quad[2].vx = coords->vx - 14;
    quad[2].vz = coords->vz - 14;
    quad[3].vx = coords->vx + 14;
    quad[3].vz = coords->vz - 14;
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -((height + (depth >> MAP_TILE_DEPTH_SHIFT)) * 12);
    if ((depth & MAP_TILE_HALF_HEIGHT_MASK) != 0) {
        rise = (depth & MAP_TILE_HALF_HEIGHT_MASK) * 6;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
        switch (slope) {
        case MAP_SLOPE_EDGE_EAST: /* edge up to east */
            quad[0].vy = quad[2].vy = quad[2].vy + rise;
            quad[1].vy = quad[3].vy = quad[3].vy - rise;
            break;
        case MAP_SLOPE_EDGE_WEST: /* edge up to west */
            quad[1].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[2].vy = quad[2].vy - rise;
            break;
        case MAP_SLOPE_EDGE_SOUTH: /* edge up to south */
            quad[0].vy = quad[1].vy = quad[1].vy + rise;
            quad[2].vy = quad[3].vy = quad[3].vy - rise;
            break;
        case MAP_SLOPE_EDGE_NORTH: /* edge up to north */
            quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[1].vy = quad[1].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_EAST: /* low corner, north-east up */
            split = 1;
            quad[0].vy = quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[1].vy = quad[1].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_EAST: /* low corner, south-east up */
            split = 2;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[2].vy + rise;
            quad[3].vy = quad[3].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_WEST: /* low corner, south-west up */
            split = 1;
            quad[0].vy = quad[1].vy = quad[3].vy = quad[3].vy + rise;
            quad[2].vy = quad[2].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_WEST: /* low corner, north-west up */
            split = 2;
            quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[0].vy - rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_EAST: /* high corner, north-east up */
            split = 1;
            quad[0].vy = quad[1].vy = quad[3].vy = quad[3].vy - rise;
            quad[2].vy = quad[2].vy + rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_EAST: /* high corner, south-east up */
            split = 2;
            quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
            quad[0].vy = quad[0].vy + rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_WEST: /* high corner, south-west up */
            split = 1;
            quad[0].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
            quad[1].vy = quad[1].vy + rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_WEST: /* high corner, north-west up */
            split = 2;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[2].vy - rise;
            quad[3].vy = quad[3].vy + rise;
            break;
        }
    }
    return split;
}
