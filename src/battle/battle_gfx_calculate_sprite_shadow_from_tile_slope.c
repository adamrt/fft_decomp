#include "fft/battle.h"
#include "fft/map.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Offsets of the unit inside its 28-unit tile along X and Z. */
#define TILE_OFFSET_X ((s16)(coords.vx % 28))
#define TILE_OFFSET_Z ((s16)(coords.vz % 28))

void battle_gfx_calculate_sprite_shadow_from_tile_slope(battle_unit_misc_data_t* unit) {
    SVECTOR coords;
    SVECTOR unused; /* unreferenced; keeps the 0x30-byte target frame */
    map_tile_t* tile;
    SVECTOR* quad;
    u8 layer;
    u8 height;
    u8 depth;
    u8 slope_type;
    s32 slope;
    s16 tx;
    s16 ty;
    s32 lhs;
    s32 rhs;
    s32 rise;

    layer = unit->map_z;
    battle_unit_copy_misc_data_xyz_values(&coords, unit);
    tile = battle_map_get_tile_data_pointer((s16)(coords.vx / 28), (s16)(coords.vz / 28), layer);
    depth = tile->depth_half_height;
    height = tile->height;
    slope_type = tile->slope_type;
    quad = unit->shadow_quad;
    quad[0].vx = coords.vx - 10;
    quad[0].vz = coords.vz + 10;
    quad[1].vx = coords.vx + 10;
    quad[1].vz = coords.vz + 10;
    quad[2].vx = coords.vx - 10;
    quad[2].vz = coords.vz - 10;
    quad[3].vx = coords.vx + 10;
    quad[3].vz = coords.vz - 10;
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -((u8)(height + (depth >> MAP_TILE_DEPTH_SHIFT)) * 12);
    slope = depth & MAP_TILE_HALF_HEIGHT_MASK;
    if (slope == 0) {
        return;
    }
    switch (slope_type) {
    case 0x52: /* edge up to east */
        tx = coords.vx % 28;
        rise = tx * 12;
        quad[0].vy = quad[1].vy = quad[3].vy = quad[2].vy = quad[3].vy - rise * slope / 28;
        quad[0].vy = quad[2].vy = quad[2].vy + 6;
        quad[1].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case 0x58: /* edge up to west */
        tx = coords.vx % 28;
        rise = tx * 12;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        quad[1].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[2].vy = quad[2].vy - 6;
        break;
    case 0x25: /* edge up to south */
        ty = coords.vz % 28;
        rise = ty * 12;
        quad[0].vy = quad[2].vy = quad[3].vy = quad[1].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        quad[0].vy = quad[1].vy = quad[1].vy + 6;
        quad[2].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case 0x85: /* edge up to north */
        ty = coords.vz % 28;
        rise = ty * 12;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        quad[2].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[1].vy = quad[1].vy - 6;
        break;
    case 0x41: /* low corner, north-east up */
        lhs = TILE_OFFSET_X;
        rhs = TILE_OFFSET_Z;
        if (lhs < rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        }
        break;
    case 0x11: /* low corner, south-east up */
        lhs = TILE_OFFSET_X;
        rhs = 28 - TILE_OFFSET_Z;
        if (lhs >= rhs) {
            rise = TILE_OFFSET_Z * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        } else {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        }
        break;
    case 0x14: /* low corner, south-west up */
        lhs = 28 - TILE_OFFSET_X;
        rhs = 28 - TILE_OFFSET_Z;
        if (lhs >= rhs) {
            rise = TILE_OFFSET_Z * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        } else {
            rise = TILE_OFFSET_X * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        }
        break;
    case 0x44: /* low corner, north-west up */
        lhs = 28 - TILE_OFFSET_X;
        rhs = TILE_OFFSET_Z;
        if (lhs >= rhs) {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        } else {
            rise = TILE_OFFSET_X * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        }
        break;
    case 0x96: /* high corner, north-east up */
        lhs = TILE_OFFSET_X;
        rhs = TILE_OFFSET_Z;
        if (lhs >= rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        }
        break;
    case 0x66: /* high corner, south-east up */
        lhs = TILE_OFFSET_X;
        rhs = 28 - TILE_OFFSET_Z;
        if (lhs >= rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        } else {
            rise = TILE_OFFSET_Z * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        }
        break;
    case 0x69: /* high corner, south-west up */
        lhs = 28 - TILE_OFFSET_X;
        rhs = 28 - TILE_OFFSET_Z;
        if (lhs < rhs) {
            rise = TILE_OFFSET_Z * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        } else {
            rise = TILE_OFFSET_X * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        }
        break;
    case 0x99: /* high corner, north-west up */
        lhs = 28 - TILE_OFFSET_X;
        rhs = TILE_OFFSET_Z;
        if (lhs >= rhs) {
            rise = TILE_OFFSET_X * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / 28);
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / 28;
        }
        break;
    }
}
