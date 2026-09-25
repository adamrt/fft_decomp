#include "fft/battle.h"
#include "psx/types.h"

/* Offsets of the point inside its 28-unit tile along X and Z. */
#define TILE_OFFSET_X ((s16)(coords[0] % 28))
#define TILE_OFFSET_Z ((s16)(coords[2] % 28))
/* Surface height for lhs slope rising (UP) or falling (DOWN) across the tile. */
#define SLOPE_PRODUCT(f) ((f) * (half * 12))
#define SLOPE_UP(f)      (base - SLOPE_PRODUCT(f) / 28)
#define SLOPE_DOWN(f)    (base - (half * 12 - SLOPE_PRODUCT(f) / 28))

/*
 * Return the negated terrain height, in screen units, under lhs point.
 *
 * coords[0] and coords[2] are the X and Z positions (tile index * 28 + offset). A tile without a
 * slope half-height, or with an unrecognised slope type, yields
 * -(height * 12); the recognised types interpolate half * 12 across the tile
 * along X, Z, or whichever of the two the corner type selects.
 *
 * The two-axis comparands must be the lhs/rhs variables while the offsets
 * used only by the formulas stay expressions: that split reproduces the
 * target's register allocation.
 */
s32 battle_map_calculate_slope_height(const s16* coords, u8 map_z) {
    s16 tile_pos[2];
    map_tile_t* tile;
    s32 half;
    s32 base;
    s16 result;
    s32 lhs;
    s32 rhs;

    tile_pos[0] = coords[0] / 28;
    tile_pos[1] = coords[2] / 28;
    tile = battle_map_get_tile_data_pointer(tile_pos[0], tile_pos[1], map_z);
    base = -(tile->height * 12);
    half = tile->depth_half_height & 0x1f;
    result = base;
    if (half != 0) {
        switch (tile->slope_type) {
        case 0x52:
            result = SLOPE_UP(TILE_OFFSET_X);
            break;
        case 0x58:
            result = SLOPE_DOWN(TILE_OFFSET_X);
            break;
        case 0x25:
            result = SLOPE_DOWN(TILE_OFFSET_Z);
            break;
        case 0x85:
            result = SLOPE_UP(TILE_OFFSET_Z);
            break;
        case 0x41:
            lhs = TILE_OFFSET_X;
            rhs = TILE_OFFSET_Z;
            if (lhs < rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        case 0x11:
            lhs = TILE_OFFSET_X;
            rhs = 28 - TILE_OFFSET_Z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(TILE_OFFSET_Z);
            } else {
                result = SLOPE_UP(lhs);
            }
            break;
        case 0x14:
            lhs = 28 - TILE_OFFSET_X;
            rhs = 28 - TILE_OFFSET_Z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(TILE_OFFSET_Z);
            } else {
                result = SLOPE_DOWN(TILE_OFFSET_X);
            }
            break;
        case 0x44:
            lhs = 28 - TILE_OFFSET_X;
            rhs = TILE_OFFSET_Z;
            if (lhs >= rhs) {
                result = SLOPE_UP(rhs);
            } else {
                result = SLOPE_DOWN(TILE_OFFSET_X);
            }
            break;
        case 0x96:
            lhs = TILE_OFFSET_X;
            rhs = TILE_OFFSET_Z;
            if (lhs >= rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        case 0x66:
            lhs = TILE_OFFSET_X;
            rhs = 28 - TILE_OFFSET_Z;
            if (lhs >= rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_DOWN(TILE_OFFSET_Z);
            }
            break;
        case 0x69:
            lhs = 28 - TILE_OFFSET_X;
            rhs = 28 - TILE_OFFSET_Z;
            if (lhs < rhs) {
                result = SLOPE_DOWN(TILE_OFFSET_Z);
            } else {
                result = SLOPE_DOWN(TILE_OFFSET_X);
            }
            break;
        case 0x99:
            lhs = 28 - TILE_OFFSET_X;
            rhs = TILE_OFFSET_Z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(TILE_OFFSET_X);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        }
    }
    return result;
}
