/* Overlays drawn on units and tiles, including the cursor polygon, world heights, shadows, direction arrows, status
 * bubbles and action result numbers, with their zoom curves. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_gfx_resource.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_animation.h"
#include "fft/battle_unit_path.h"
#include "fft/battle_unit_sprite.h"
#include "fft/data_status.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/map.h"
#include "fft/menu_option.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Texture coordinates for the status bubble graphics, indexed by status bubble
 * id (0x800949dc/0x800949f4) and by a counter digit (0x80094a0c/0x80094a24).
 * Each table holds 24 bytes; the pairs are read together as one u/v. */
static u8 g_battle_status_bubble_u[24]
    = { 0, 16, 30, 44, 58, 72, 86, 100, 114, 128, 142, 0, 16, 30, 44, 58, 72, 86, 100, 86, 0, 0, 0, 0 };

static u8 g_battle_status_bubble_v[24] = { 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 188, 188, 188, 188,
    188, 188, 188, 188, 200, 176, 0, 0, 0 };

static u8 g_battle_status_bubble_digit_u[24]
    = { 142, 128, 114, 100, 86, 142, 128, 142, 128, 114, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static u8 g_battle_status_bubble_digit_v[24]
    = { 200, 200, 200, 200, 200, 188, 188, 176, 176, 176, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Build the four corners of the cursor quad over the tile under coords on map layer `layer`, at
 * the tile surface and following its slope. Returns 0 for a planar quad, 1 when a corner slope
 * creases it along the v0-v3 diagonal, 2 along v1-v2. */
s32 battle_target_calculate_cursor_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad) {
    map_tile_t* tile;
    s32 split;
    s32 rise;
    u8 height;
    u8 depth;
    u8 slope;

    split = 0;
    tile = battle_map_get_tile_data_pointer((coords->vx / MAP_TILE_SIZE), (coords->vz / MAP_TILE_SIZE), layer);
    height = tile->height;
    /* Byte access retains the original packed load and field-extraction order. */
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
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -((height + (depth >> MAP_TILE_DEPTH_SHIFT)) * MAP_HEIGHT_STEP);
    if ((depth & MAP_TILE_HALF_HEIGHT_MASK) != 0) {
        /* The slope spans half_height levels: raise the quad to its middle, then
         * move the high corners up and the low corners down by half the span. */
        rise = (depth & MAP_TILE_HALF_HEIGHT_MASK) * (MAP_HEIGHT_STEP / 2);
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

/* Surface height for a slope rising (UP) or falling (DOWN) across the tile. */
#define SLOPE_PRODUCT(f) ((f) * (half * MAP_HEIGHT_STEP))
#define SLOPE_UP(f)      (base - SLOPE_PRODUCT(f) / MAP_TILE_SIZE)
#define SLOPE_DOWN(f)    (base - (half * MAP_HEIGHT_STEP - SLOPE_PRODUCT(f) / MAP_TILE_SIZE))

/*
 * Return the negated terrain height, in world units, under a point.
 *
 * coords->vx and coords->vz are the X and Z positions (tile index * 28 + offset). A tile without a
 * slope half-height, or with an unrecognized slope type, yields
 * -(height * 12); the recognized types interpolate half * 12 across the tile
 * along X, Z, or whichever of the two the corner type selects. x and z are
 * the offsets of the point inside its 28-unit tile.
 */
s32 battle_map_calculate_slope_height(const SVECTOR* coords, s32 map_z) {
    s16 tile_pos[2];
    map_tile_t* tile;
    s32 half;
    s32 base;
    s16 result;
    s16 x;
    s16 z;
    s32 lhs;
    s32 rhs;

    tile_pos[0] = coords->vx / MAP_TILE_SIZE;
    tile_pos[1] = coords->vz / MAP_TILE_SIZE;
    tile = battle_map_get_tile_data_pointer(tile_pos[0], tile_pos[1], (u8)map_z);
    base = -(tile->height * MAP_HEIGHT_STEP);
    half = tile->depth_half_height.half_height;
    result = base;
    if (half != 0) {
        switch (tile->slope_type) {
        case MAP_SLOPE_EDGE_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            result = SLOPE_UP(x);
            break;
        case MAP_SLOPE_EDGE_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            result = SLOPE_DOWN(x);
            break;
        case MAP_SLOPE_EDGE_SOUTH:
            z = coords->vz % MAP_TILE_SIZE;
            result = SLOPE_DOWN(z);
            break;
        case MAP_SLOPE_EDGE_NORTH:
            z = coords->vz % MAP_TILE_SIZE;
            result = SLOPE_UP(z);
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = z;
            if (lhs < rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(z);
            } else {
                result = SLOPE_UP(lhs);
            }
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(z);
            } else {
                result = SLOPE_DOWN(x);
            }
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = z;
            if (lhs >= rhs) {
                result = SLOPE_UP(rhs);
            } else {
                result = SLOPE_DOWN(x);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = z;
            if (lhs >= rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs >= rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_DOWN(z);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs < rhs) {
                result = SLOPE_DOWN(z);
            } else {
                result = SLOPE_DOWN(x);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(x);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        }
    }
    return result;
}

/* How far a unit stands above a water tile's floor, in half levels: the full depth for Float and
 * on-water movement, one level less for in-water movement, else 0. */
s32 battle_move_get_water_standing_offset(battle_unit_t* unit, map_tile_t* tile) {
    s32 movement_flags;

    if (unit->stats != 0) {
        movement_flags = battle_move_get_effective_flags(unit->stats);
    } else {
        movement_flags = 0;
    }
    if (tile->depth_half_height.depth) {
        if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT)
            || (movement_flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER)) {
            return tile->depth_half_height.depth * 2;
        }
        if (movement_flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) {
            return (tile->depth_half_height.depth - 1) * 2;
        }
    }
    return 0;
}

/* A rider's mount; any other unit is returned itself. */
static battle_unit_t* battle_unit_get_mount(battle_unit_t* unit) {
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        return battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
    }
    return unit;
}

/*
 * The world y (negative is up) of a unit's feet at coords: vx and vz are world
 * x and z, vy holds the map layer.
 *
 * A unit standing on another unit (standing_on_battle_id_plus_one) stands on
 * top of it, unless that unit's height is unavailable; any other unit stands
 * on the terrain surface under coords. On a water tile a unit with Float or
 * on-water movement, a treasure or a crystal stays at the water surface, a
 * chicken or frog 4 units below it, and an in-water mover or a dead unit one
 * level below it; others stand on the bottom. depth_height_offset (the rise
 * of a floating unit) then lifts every case.
 */
static s32 battle_gfx_calculate_world_height_from_coords(battle_unit_t* unit, const SVECTOR* coords) {
    battle_unit_height_data_t height;
    map_tile_t* tile;
    s32 flags;
    s16 world_height;
    s32 sprite;
    s32 animation;
    s16 raised;

    if (unit->standing_on_battle_id_plus_one != 0) {
        battle_unit_calculate_height_data(&height, unit->standing_on_battle_id_plus_one - 1);
        if (height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) {
            world_height = battle_map_calculate_slope_height(coords, coords->vy & 0xff);
        } else {
            /* total_height counts half levels. */
            world_height = -height.total_height / 2 * MAP_HEIGHT_STEP;
        }
    } else {
        world_height = battle_map_calculate_slope_height(coords, coords->vy & 0xff);
    }
    if (unit->stats != 0) {
        tile = battle_map_get_tile_data_pointer(coords->vx / MAP_TILE_SIZE, coords->vz / MAP_TILE_SIZE, coords->vy);
        if (tile->depth_half_height.depth) {
            flags = battle_move_get_effective_flags(unit->stats);
            sprite = unit->spritesheet_id;
            animation = unit->animation.sequence_id >> 1;
            if ((flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) || (flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER)) {
                world_height -= (tile->depth_half_height.depth) * MAP_HEIGHT_STEP + unit->depth_height_offset;
            } else if (sprite == BATTLE_SPRITESHEET_ID_CHICKEN || sprite == BATTLE_SPRITESHEET_ID_FROG) {
                raised = world_height + 4;
                world_height = raised - ((tile->depth_half_height.depth) * MAP_HEIGHT_STEP + unit->depth_height_offset);
            } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) || animation == BATTLE_UNIT_ANIMATION_DEAD) {
                world_height -= ((tile->depth_half_height.depth) - 1) * MAP_HEIGHT_STEP + unit->depth_height_offset;
            } else if (sprite == BATTLE_SPRITESHEET_ID_TREASURE || sprite == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                world_height -= (tile->depth_half_height.depth) * MAP_HEIGHT_STEP + unit->depth_height_offset;
            } else {
                world_height -= unit->depth_height_offset;
            }
        } else {
            world_height -= unit->depth_height_offset;
        }
    } else {
        world_height -= unit->depth_height_offset;
    }
    return world_height;
}

/* battle_gfx_calculate_world_height_from_coords at the caller's coords (vy
 * holds the map layer), evaluated for the mount when the unit is a rider. A
 * rider's result is moved by -10 plus the mount's float bob offset.
 *
 * Here and in the three functions below, the separate adjusted_world_height
 * step keeps the target's order of operations. */
s32 battle_gfx_calculate_world_height_with_caller_coords(battle_unit_t* unit, const SVECTOR* coords) {
    battle_unit_t* mount = battle_unit_get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height = battle_gfx_calculate_world_height_from_coords(mount, coords);

    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* battle_gfx_calculate_world_height_from_coords at the center of the movement
 * destination tile of the unit, or of its mount for a rider. A rider's result is
 * moved by -10 plus the mount's float bob offset. */
s32 battle_gfx_calculate_world_height_from_move_destination(battle_unit_t* unit) {
    SVECTOR coord;
    battle_unit_t* mount = battle_unit_get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    coord.vx = mount->movement.destination_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vz = mount->movement.destination_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->movement.destination_z;
    world_height = battle_gfx_calculate_world_height_from_coords(mount, &coord);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* battle_gfx_calculate_world_height_from_coords at the current x and map-depth
 * position of the unit, or of its mount for a rider, on the mount's map layer. A
 * rider's result is moved by -10 plus the mount's float bob offset. */
s32 battle_gfx_calculate_world_height_from_world_coords(battle_unit_t* unit) {
    SVECTOR coord;
    battle_unit_t* mount = battle_unit_get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    /* The target loads the s16 world coordinates unsigned (lhu). */
    coord.vx = mount->world_coords.vx;
    coord.vz = mount->world_coords.vz;
    coord.vy = mount->map_z;
    world_height = battle_gfx_calculate_world_height_from_coords(mount, &coord);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* battle_gfx_calculate_world_height_from_coords at the center of the map tile
 * of the unit, or of its mount for a rider. A rider's result is moved by -10 plus
 * the mount's float bob offset. */
s32 battle_gfx_calculate_world_height_from_unit_tile(battle_unit_t* unit) {
    SVECTOR coord;
    battle_unit_t* mount = battle_unit_get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    coord.vx = mount->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vz = mount->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->map_z;
    world_height = battle_gfx_calculate_world_height_from_coords(mount, &coord);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* battle_gfx_calculate_world_height_from_coords at the center of the tile
 * recorded in the battle stats of the unit, or of its mount for a rider, on the
 * mount's map layer. A rider's result is moved by -10 plus the mount's float bob
 * offset. */
s32 battle_gfx_calculate_world_height_from_battle_tile(battle_unit_t* unit) {
    SVECTOR coord;
    unit_stats_t* stats;
    battle_unit_t* mount = battle_unit_get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    stats = mount->stats;
    coord.vx = stats->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    stats = mount->stats;
    coord.vz = stats->position.y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->map_z;
    world_height = battle_gfx_calculate_world_height_from_coords(mount, &coord);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* Queue a unit's ground shadow quad.
 *
 * The four shadow_quad corners are projected into a semi-transparent POLY_FT4.
 * A unit below the water surface of its tile (world y greater than
 * -(height + depth) * 12, negative y being up) gets the additive (tpage 0x3f)
 * water image, whose UVs turn a quarter every 16 counts of shadow_counter
 * (advanced by the frame interval); any other unit gets the subtractive
 * (tpage 0x5f) shadow image.
 *
 * A unit at or below the terrain under its event and SEQ adjusted position uses
 * the caller's ordering-table entry. A unit above it (jumping, floating or on
 * water) is sorted at the depth of the ground or water surface below it.
 *
 * The dirty test sets bit 0 of shadow_dirty before testing it, so the quad is
 * recalculated on every call.
 *
 * `unused` reproduces the target's 0x58-byte frame. */
void battle_gfx_draw_unit_shadow(battle_unit_t* unit, u32* ot) {
    VECTOR transformed;
    SVECTOR position;
    SVECTOR offset;
    SVECTOR unused;
    long flag;
    POLY_FT4* prim;
    map_tile_t* tile;
    SVECTOR* corners;
    u32* packet;
    s32 depth;
    u32 frame;

    if ((unit->shadow_dirty |= 1) != 0) {
        battle_gfx_calculate_sprite_shadow_from_tile_slope(unit);
        unit->shadow_dirty ^= 1;
    }
    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    prim = g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim();
    corners = unit->shadow_quad;
    ((P_TAG*)prim)->len = 9;
    prim->code = 0x2c;
    RotTrans(&corners[0], &transformed, &flag);
    prim->x0 = transformed.vx;
    prim->y0 = transformed.vy;
    RotTrans(&corners[1], &transformed, &flag);
    prim->x1 = transformed.vx;
    prim->y1 = transformed.vy;
    RotTrans(&corners[2], &transformed, &flag);
    prim->x2 = transformed.vx;
    prim->y2 = transformed.vy;
    RotTrans(&corners[3], &transformed, &flag);
    prim->x3 = transformed.vx;
    prim->y3 = transformed.vy;
    prim->code |= 2;
    tile = battle_map_get_tile_data_pointer(
        (unit->world_coords.vx / MAP_TILE_SIZE), (unit->world_coords.vz / MAP_TILE_SIZE), unit->map_z);
    if (tile->depth_half_height.depth
        && -(tile->height + tile->depth_half_height.depth) * MAP_HEIGHT_STEP < unit->world_coords.vy) {
        unit->shadow_counter += g_main_system_vsync_interval;
        frame = unit->shadow_counter;
        prim->tpage = 0x3f;
        prim->clut = unit->clut + 0x40; /* the unit's CLUT of MAP_CLUT_ROW_UNIT_SHADOWS */
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        switch (frame & 0x30) {
        case 0:
            prim->u0 = prim->u2 = 0xa8;
            prim->u1 = prim->u3 = 0xbb;
            prim->v0 = prim->v1 = 0x40;
            prim->v2 = prim->v3 = 0x53;
            break;
        case 0x10:
            prim->u2 = prim->u3 = 0xa8;
            prim->u0 = prim->u1 = 0xbb;
            prim->v0 = prim->v2 = 0x40;
            prim->v1 = prim->v3 = 0x53;
            break;
        case 0x20:
            prim->u1 = prim->u3 = 0xa8;
            prim->u0 = prim->u2 = 0xbb;
            prim->v2 = prim->v3 = 0x40;
            prim->v0 = prim->v1 = 0x53;
            break;
        case 0x30:
            prim->u0 = prim->u1 = 0xa8;
            prim->u2 = prim->u3 = 0xbb;
            prim->v1 = prim->v3 = 0x40;
            prim->v0 = prim->v2 = 0x53;
            break;
        }
    } else {
        prim->tpage = 0x5f;
        prim->clut = unit->clut + 0x40; /* the unit's CLUT of MAP_CLUT_ROW_UNIT_SHADOWS */
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        prim->u0 = prim->u2 = 0x90;
        prim->u1 = prim->u3 = 0xa3;
        prim->v0 = prim->v1 = 0x40;
        prim->v2 = prim->v3 = 0x53;
    }
    main_util_set_svector(&position, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx, 0,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
    position.vy = battle_map_calculate_slope_height(&position, unit->map_z);
    if (unit->world_coords.vy >= position.vy) {
        packet = ot;
    } else {
        tile = battle_map_get_tile_data_pointer(
            (position.vx / MAP_TILE_SIZE), (position.vz / MAP_TILE_SIZE), unit->map_z);
        position.vy -= tile->depth_half_height.depth * MAP_HEIGHT_STEP;
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if (tile->surface.depth_bias) {
                battle_camera_calculate_relative_offset_by_5(&position, &offset);
            } else {
                battle_camera_calculate_relative_offset_by_1(&position, &offset);
            }
        } else if (tile->surface.depth_bias) {
            battle_camera_convert_screen_coords_modify_by_5(&position, &offset);
        } else {
            battle_camera_convert_screen_coords_modify_by_1(&position, &offset);
        }
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        RotTrans(&offset, &transformed, &flag);
        depth = transformed.vz / 4;
        packet = main_gfx_get_otag() + depth;
    }
    AddPrim(packet, prim);
}

/*
 * Queue the glow primitives for one cursor tile.
 *
 * The four vertices are projected with the battle camera. Shape 0 (see
 * battle_target_calculate_cursor_tile_polygon) draws a textured quad; shapes 1
 * and 2 split the tile into two textured triangles along the v0-v3 or v1-v2
 * diagonal. Each primitive draws the 13 x 13 glow image additively (tpage 0x3f)
 * with CLUT palette + 1 of the sprite row, and a copy of it fills the same area
 * from a single texel with CLUT 5 at half blend (tpage 0x1f). Both are queued
 * only when the first three projected vertices face the camera. Every
 * primitive takes a POLY_FT4-sized slot of the pool.
 */
void battle_gfx_build_cursor_tile_glow(
    s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot) {
    VECTOR screen;
    MATRIX unused_matrix; /* unreferenced; reserves the target frame's 0x20-byte gap at sp+0x20 */
    long flag;
    POLY_FT4* quad;
    POLY_FT4* quad_glow;
    POLY_FT3* tri_a;
    POLY_FT3* tri_b;
    POLY_FT3* tri_a_glow;
    POLY_FT3* tri_b_glow;

    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    switch (shape) {
    case 0:
        quad = g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim();
        quad_glow = g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim();
        ((P_TAG*)quad)->len = 9;
        quad->code = 0x2c;
        RotTrans(v0, &screen, &flag);
        quad->x0 = screen.vx;
        quad->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        quad->x1 = screen.vx;
        quad->y1 = screen.vy;
        RotTrans(v2, &screen, &flag);
        quad->x2 = screen.vx;
        quad->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        quad->x3 = screen.vx;
        quad->y3 = screen.vy;
        quad->code |= 2;
        quad->tpage = 0x3f;
        quad->clut = GetClut(palette * 16 + 16, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        quad->r0 = quad->g0 = quad->b0 = 0x80;
        quad->u2 = 0;
        quad->u0 = 0;
        quad->u3 = 0xd;
        quad->u1 = 0xd;
        quad->v1 = 0xa0;
        quad->v0 = 0xa0;
        quad->v3 = 0xad;
        quad->v2 = 0xad;
        *quad_glow = *quad;
        quad_glow->tpage = 0x1f;
        quad_glow->clut = GetClut(0x50, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        quad_glow->u2 = 0xc;
        quad_glow->u0 = 0xc;
        quad_glow->u3 = 0xc;
        quad_glow->u1 = 0xc;
        quad_glow->v1 = 0xa0;
        quad_glow->v0 = 0xa0;
        quad_glow->v3 = 0xa0;
        quad_glow->v2 = 0xa0;
        if (NormalClip((quad->y0 << 16) + quad->x0, (quad->y1 << 16) + quad->x1, (quad->y2 << 16) + quad->x2) >= 0) {
            AddPrim(ot, quad);
            AddPrim(ot, quad_glow);
        }
        return;
    case 1:
        tri_a = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        tri_b = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        tri_a_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        tri_b_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        ((P_TAG*)tri_a)->len = 7;
        tri_a->code = 0x24;
        ((P_TAG*)tri_b)->len = 7;
        tri_b->code = 0x24;
        RotTrans(v0, &screen, &flag);
        tri_a->x0 = screen.vx;
        tri_a->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        tri_a->x1 = tri_b->x0 = screen.vx;
        tri_a->y1 = tri_b->y0 = screen.vy;
        RotTrans(v2, &screen, &flag);
        tri_a->x2 = tri_b->x2 = screen.vx;
        tri_a->y2 = tri_b->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        tri_b->x1 = screen.vx;
        tri_b->y1 = screen.vy;
        tri_a->code |= 2;
        tri_b->code |= 2;
        tri_a->tpage = tri_b->tpage = 0x3f;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b->r0 = tri_b->g0 = tri_b->b0 = 0x80;
        tri_a->r0 = tri_a->g0 = tri_a->b0 = 0x80;
        tri_b->u2 = 0;
        tri_a->u2 = 0;
        tri_a->u0 = 0;
        tri_b->u1 = 0xd;
        tri_b->u0 = 0xd;
        tri_a->u1 = 0xd;
        tri_b->v0 = 0xa0;
        tri_a->v1 = 0xa0;
        tri_a->v0 = 0xa0;
        tri_b->v2 = 0xad;
        tri_b->v1 = 0xad;
        tri_a->v2 = 0xad;
        *tri_a_glow = *tri_a;
        *tri_b_glow = *tri_b;
        tri_a_glow->tpage = tri_b_glow->tpage = 0x1f;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b_glow->u2 = 0xc;
        tri_a_glow->u2 = 0xc;
        tri_a_glow->u0 = 0xc;
        tri_b_glow->u1 = 0xc;
        tri_b_glow->u0 = 0xc;
        tri_a_glow->u1 = 0xc;
        tri_b_glow->v0 = 0xa0;
        tri_a_glow->v1 = 0xa0;
        tri_a_glow->v0 = 0xa0;
        tri_b_glow->v2 = 0xa0;
        tri_b_glow->v1 = 0xa0;
        tri_a_glow->v2 = 0xa0;
        if (NormalClip((tri_a->y0 << 16) + tri_a->x0, (tri_a->y1 << 16) + tri_a->x1, (tri_a->y2 << 16) + tri_a->x2)
            >= 0) {
            AddPrim(ot, tri_a);
            AddPrim(ot, tri_a_glow);
        }
        if (NormalClip((tri_b->y0 << 16) + tri_b->x0, (tri_b->y1 << 16) + tri_b->x1, (tri_b->y2 << 16) + tri_b->x2)
            >= 0) {
            AddPrim(ot, tri_b);
            AddPrim(ot, tri_b_glow);
        }
        break;
    case 2:
        tri_a = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        tri_b = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        tri_a_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        tri_b_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim());
        ((P_TAG*)tri_a)->len = 7;
        tri_a->code = 0x24;
        ((P_TAG*)tri_b)->len = 7;
        tri_b->code = 0x24;
        RotTrans(v0, &screen, &flag);
        tri_a->x0 = tri_b->x0 = screen.vx;
        tri_a->y0 = tri_b->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        tri_a->x1 = screen.vx;
        tri_a->y1 = screen.vy;
        RotTrans(v2, &screen, &flag);
        tri_b->x2 = screen.vx;
        tri_b->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        tri_a->x2 = tri_b->x1 = screen.vx;
        tri_a->y2 = tri_b->y1 = screen.vy;
        tri_a->code |= 2;
        tri_b->code |= 2;
        tri_a->tpage = tri_b->tpage = 0x3f;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b->r0 = tri_b->g0 = tri_b->b0 = 0x80;
        tri_a->r0 = tri_a->g0 = tri_a->b0 = 0x80;
        tri_b->u2 = 0;
        tri_b->u0 = 0;
        tri_a->u0 = 0;
        tri_b->u1 = 0xd;
        tri_a->u2 = 0xd;
        tri_a->u1 = 0xd;
        tri_b->v0 = 0xa0;
        tri_a->v1 = 0xa0;
        tri_a->v0 = 0xa0;
        tri_b->v2 = 0xad;
        tri_b->v1 = 0xad;
        tri_a->v2 = 0xad;
        *tri_a_glow = *tri_a;
        *tri_b_glow = *tri_b;
        tri_a_glow->tpage = tri_b_glow->tpage = 0x1f;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b_glow->u2 = 0xc;
        tri_a_glow->u2 = 0xc;
        tri_a_glow->u0 = 0xc;
        tri_b_glow->u1 = 0xc;
        tri_b_glow->u0 = 0xc;
        tri_a_glow->u1 = 0xc;
        tri_b_glow->v0 = 0xa0;
        tri_a_glow->v1 = 0xa0;
        tri_a_glow->v0 = 0xa0;
        tri_b_glow->v2 = 0xa0;
        tri_b_glow->v1 = 0xa0;
        tri_a_glow->v2 = 0xa0;
        if (NormalClip((tri_a->y0 << 16) + tri_a->x0, (tri_a->y1 << 16) + tri_a->x1, (tri_a->y2 << 16) + tri_a->x2)
            >= 0) {
            AddPrim(ot, tri_a);
            AddPrim(ot, tri_a_glow);
        }
        if (NormalClip((tri_b->y0 << 16) + tri_b->x0, (tri_b->y1 << 16) + tri_b->x1, (tri_b->y2 << 16) + tri_b->x2)
            >= 0) {
            AddPrim(ot, tri_b);
            AddPrim(ot, tri_b_glow);
        }
        break;
    }
}

/*
 * Queue the map selection cursor over the cursor tile, whose surface point the
 * caller passes in `base` with the camera matrices loaded.
 *
 * The cursor floats 0x3c units above the tile, or 0x18 above the sprite height
 * of the unit the cursor selects there, scaled by the camera zoom. It bobs
 * down through an eight-frame pattern, advanced by the frame interval unless
 * the status screen is open. Its quad is the 16 x 24 cursor image scaled by
 * the camera zoom, sorted at the depth of the tile's surface point.
 *
 * The two initialized locals occupy 64 bytes of .rodata at 0x80067798: the bob
 * offsets and their durations in frames.
 */
void battle_gfx_draw_map_selection_cursor(SVECTOR* base, u32* ot) {
    VECTOR transformed;
    SVECTOR screen;
    s32 bob[8] = { 0, 1, 2, 3, 5, 3, 2, 1 };
    s32 duration[8] = { 16, 8, 2, 2, 6, 4, 4, 10 };
    long flag;
    POLY_FT4* prim;
    battle_unit_t* unit;
    s32 height;
    s32 half_height;

    prim = g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim();
    ((P_TAG*)prim)->len = 9;
    prim->code = 0x2c;
    if (g_battle_menu_status_screen_selected != 1) {
        g_battle_gfx_selection_cursor_bob_timer += g_main_system_vsync_interval;
        if (g_battle_gfx_selection_cursor_bob_timer >= duration[g_battle_gfx_selection_cursor_bob_frame]) {
            g_battle_gfx_selection_cursor_bob_frame += 1;
            g_battle_gfx_selection_cursor_bob_timer = 0;
            if (g_battle_gfx_selection_cursor_bob_frame >= 8) {
                g_battle_gfx_selection_cursor_bob_frame = 0;
            }
        }
    }
    unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit != 0) {
        height
            = g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height * g_battle_camera_zoom.vy / ONE + 0x18;
    } else {
        height = 0x3c;
    }
    base->vy -= height - bob[g_battle_gfx_selection_cursor_bob_frame];
    RotTrans(base, &transformed, &flag);

    prim->x0 = transformed.vx - g_battle_camera_zoom.vx / 512;
    prim->y0 = transformed.vy - g_battle_camera_zoom.vy * 12 / ONE;
    prim->x1 = transformed.vx + g_battle_camera_zoom.vx / 512;
    prim->y1 = transformed.vy - g_battle_camera_zoom.vy * 12 / ONE;
    prim->x2 = transformed.vx - g_battle_camera_zoom.vx / 512;
    prim->y2 = transformed.vy + g_battle_camera_zoom.vy * 12 / ONE;
    prim->x3 = transformed.vx + g_battle_camera_zoom.vx / 512;
    half_height = g_battle_camera_zoom.vy * 12 / ONE;
    {
        /* Read as a halfword, as the target does, after the half height. */
        u16 screen_y = transformed.vy;
        prim->tpage = 0x5f;
        prim->y3 = screen_y + half_height;
    }
    prim->code |= 2;
    prim->clut = GetClut(0x40, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
    prim->r0 = prim->g0 = prim->b0 = 0x80;
    prim->u2 = 0x40;
    prim->u0 = 0x40;
    prim->u3 = 0x50;
    prim->u1 = 0x50;
    prim->v1 = 0x80;
    prim->v0 = 0x80;
    prim->v3 = 0x98;
    prim->v2 = 0x98;
    battle_camera_convert_screen_coords_modify_by_1(base, &screen);
    RotTrans(&screen, &transformed, &flag);
    AddPrim(ot + (transformed.vz / 4), prim);
}

/*
 * Queue the four wait-direction arrows around a unit's feet. The arrow of the
 * source unit's facing quadrant blinks between two lit images every 32 timer
 * counts; the others show the unlit image. Called from
 * battle_gfx_draw_wait_direction_unit with the unit's base position and the
 * ordering-table slot for its depth.
 *
 * The two initialized locals occupy 32 bytes of .rodata at 0x800677d8; the
 * arrow offsets are the four unit steps of length 14 around the tile.
 *
 * Each arm repeats the four V stores because the target hoists 0x9a and 0xa8
 * into saved registers, which only happens with more than one use apiece;
 * cross-jumping merges the repeats back into one tail.
 */
void battle_gfx_draw_wait_direction_arrows(SVECTOR* base, u32* ot) {
    SVECTOR position;
    VECTOR transformed;
    s32 dx[4] = { 0, -14, 0, 14 };
    s32 dz[4] = { -14, 0, 14, 0 };
    long flag;
    battle_unit_t* unit;
    POLY_FT4* prim;
    /* Signed 16-bit value: the target's division emits the `bgez`/`addiu 0x3ff`
     * correction, so the quadrant bits are not held in an unsigned type. */
    s16 facing_bits;
    s32 i;

    unit = battle_unit_get_source();
    g_battle_gfx_wait_arrow_blink_timer += g_main_system_vsync_interval;
    for (i = 0; i < 4; i++) {
        prim = g_battle_gfx_sprite_prim_pool + battle_gfx_take_sprite_prim();
        ((P_TAG*)prim)->len = 9;
        prim->code = 0x2e;
        prim->tpage = 0x3f;
        main_util_set_svector(&position, base->vx + dx[i], base->vy - 48, base->vz + dz[i]);
        RotTrans(&position, &transformed, &flag);
        prim->x0 = prim->x2 = transformed.vx - 7;
        prim->x1 = prim->x3 = transformed.vx + 7;
        prim->y0 = prim->y1 = transformed.vy - 7;
        prim->y2 = prim->y3 = transformed.vy + 7;
        prim->clut = GetClut(0x30, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        facing_bits = unit->facing & 0xc00;
        if (i == facing_bits / 0x400) {
            if (g_battle_gfx_wait_arrow_blink_timer & 0x20) {
                prim->u0 = prim->u2 = 0xbc;
                prim->u1 = prim->u3 = 0xca;
                prim->v1 = 0x9a;
                prim->v0 = 0x9a;
                prim->v3 = 0xa8;
                prim->v2 = 0xa8;
            } else {
                prim->u0 = prim->u2 = 0xd6;
                prim->u1 = prim->u3 = 0xe4;
                prim->v1 = 0x9a;
                prim->v0 = 0x9a;
                prim->v3 = 0xa8;
                prim->v2 = 0xa8;
            }
        } else {
            prim->u0 = prim->u2 = 0xca;
            prim->u1 = prim->u3 = 0xd8;
            prim->v1 = 0x9a;
            prim->v0 = 0x9a;
            prim->v3 = 0xa8;
            prim->v2 = 0xa8;
        }
        AddPrim(ot, prim);
    }
}

/* g_battle_unit_float_bob_offsets: read-only table of this object (24 bytes at 0x800677f8), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_battle_unit_float_bob_offsets_rodata[6] = {
    0x04040404,
    0x02030303,
    0x01010102,
    0x00000000,
    0x02010101,
    0x03030302,
};

/* battle_gfx_calculate_world_height_from_world_coords is called as returning s16; the target's
 * handling of the result needs it, and a plain s32 call compiles differently. */
typedef s16 (*battle_screen_z_halfword_fn_t)(battle_unit_t* unit);

/* Raise a floating unit's height offset, or lower it back to the ground.
 *
 * A floating unit rises by the frame interval each update up to 12, then bobs through
 * g_battle_unit_float_bob_offsets (24 entries, two frames each) unless it is
 * a mount's rider. Otherwise the offset decays to 0, except during the
 * crystal-learn state. The unit's world and real y are then raised to the
 * recalculated ground height when they lie below it. Jumping units whose
 * height is controlled by their jump animation are left untouched. */
static void battle_unit_increment_or_decrement_height_mod(battle_unit_t* unit) {
    s8 wave[24];
    s16 ground_y;
    u32 phase;

    memcpy(wave, g_battle_unit_float_bob_offsets, sizeof(wave));
    if (unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
        return;
    }
    if (unit->movement.floating) {
        if (unit->depth_height_offset < 12) {
            unit->depth_height_offset += g_main_system_vsync_interval;
        } else if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_RIDER) {
            phase = unit->float_bob_phase += g_main_system_vsync_interval;
            unit->float_bob_offset = wave[phase % 48 / 2];
        } else {
            unit->float_bob_offset = 0;
        }
    } else if (g_battle_game_state != BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        if (unit->depth_height_offset != 0) {
            unit->depth_height_offset -= g_main_system_vsync_interval;
            if (unit->depth_height_offset < 0) {
                unit->depth_height_offset = 0;
            }
            ground_y = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_world_height_from_world_coords)(unit);
            unit->world_coords.vy = ground_y;
            unit->real.vy = ground_y * ONE;
        } else {
            unit->float_bob_offset = 0;
        }
    }
    ground_y = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_world_height_from_world_coords)(unit);
    if (unit->world_coords.vy > ground_y) {
        unit->world_coords.vy = ground_y;
        unit->real.vy = ground_y * ONE;
    }
}

/* Advances one unit's status bubble by g_main_system_vsync_interval frames.
 *
 * The bubble switches texture rows every 16 timer ticks (bubble 9 every 8).
 * Every 16 ticks the bubble id walks forward to the next of the 22 bubbles
 * that applies, wrapping to 0, and the bubble's x/y offsets are chosen from
 * the unit's SHP type and animation. Bubbles 0 to 0x14 follow statuses
 * (g_battle_gfx_status_bubble_status_masks); bubble 0x15 belongs to the source
 * unit instead. The dead (0) and Death Sentence (0x14) bubbles, which show
 * counters, stay while their status lasts. A unit with no bubble status that
 * is not the source unit loses its bubble.
 *
 * The block-scoped bubble_mask keeps GCC from rotating the loop's leading exit
 * tests to the bottom, and indexing g_battle_gfx_status_bubble_status_masks inside the loop lets loop.c
 * strength-reduce it into the target's pointer walk. */
static void battle_gfx_determine_status_bubble_parameters(battle_unit_t* unit) {
    s32 frame;
    s32 bubble;
    u32 status_flags;
    s32 animation;

    for (frame = 0; frame < g_main_system_vsync_interval; frame++) {
        bubble = unit->status_bubble_id;
        if (bubble == 9) {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 8) >> 3;
        } else {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 0x10) >> 4;
        }

        if ((unit->status_bubble_timer & 0xF) == 0) {
            status_flags = unit->status_flags_1_4;
            if (((status_flags & BATTLE_UNIT_STATUS_BUBBLE_MASK) == 0)
                && (unit->runtime_id != g_battle_unit_source_runtime_id)) {
                unit->status_bubble_active = 0;
                return;
            }

            for (;;) {
                s32 bubble_mask;

                if (bubble == 0) {
                    if (status_flags & BATTLE_UNIT_STATUS_DEAD) {
                        break;
                    }
                } else if (bubble == 0x14) {
                    if (status_flags & BATTLE_UNIT_STATUS_DEATH_SENTENCE) {
                        break;
                    }
                }
                bubble++;
                if (bubble >= 0x16) {
                    bubble = 0;
                }
                if (bubble == unit->status_bubble_id) {
                    break;
                }
                if (bubble != 0x15) {
                    bubble_mask = g_battle_gfx_status_bubble_status_masks[bubble];
                    if (status_flags & bubble_mask) {
                        break;
                    }
                } else if (unit->runtime_id == g_battle_unit_source_runtime_id) {
                    break;
                }
            }
            unit->status_bubble_id = bubble;

            switch (g_battle_gfx_spritesheet_data[unit->spritesheet_id].shp_id) {
            case BATTLE_SHP_ARUTE:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x46;
                break;
            case BATTLE_SHP_KANZEN:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x78;
                break;
            case BATTLE_SHP_OTHER:
                animation = unit->animation.sequence_id >> 1;
                if (animation == BATTLE_UNIT_ANIMATION_DEAD || animation == BATTLE_UNIT_ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x19;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x19;
                }
                break;
            case BATTLE_SHP_TYPE1:
            case BATTLE_SHP_TYPE2:
                if (battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z)->depth_half_height.depth) {
                    switch (unit->animation.sequence_id >> 1) {
                    case BATTLE_UNIT_ANIMATION_DEAD:
                    case BATTLE_UNIT_ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x23;
                        break;
                    case BATTLE_UNIT_ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                } else {
                    switch (unit->animation.sequence_id >> 1) {
                    case BATTLE_UNIT_ANIMATION_DEAD:
                    case BATTLE_UNIT_ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x19;
                        break;
                    case BATTLE_UNIT_ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                }
                break;
            default:
                animation = unit->animation.sequence_id >> 1;
                if (animation == BATTLE_UNIT_ANIMATION_DEAD || animation == BATTLE_UNIT_ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x1E;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x32;
                }
                break;
            }
        }
        unit->status_bubble_timer++;
    }
}

/* g_battle_status_bubble_zoom: read-only table of this object (8 bytes at 0x80067830), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_status_bubble_zoom_rodata[2] = {
    0x10001000,
    0x00001000,
};

/*
 * Queue the unit's status bubble into the ordering table.
 *
 * The bubble's texture coordinates are chosen from the status bubble id. The
 * dead bubble (id 0) shows the Reraise image (entry 10) on a unit with
 * Reraise, else the digit of its death counter, else (counter 0xff) its own
 * image. The Death Sentence bubble (id 20) shows the digit of that status's
 * count. Ids 0, 9, 10 and 21 have a second texture row, selected by
 * status_bubble_alternate_row. The bubble is drawn at status_bubble_y (negative
 * is up) from the unit's feet and status_bubble_x across, one pixel higher
 * while bit 4 of the timer is set, and sorted at the depth of a point 12 units
 * below its base.
 */
void battle_gfx_draw_status_bubble(battle_unit_t* unit, const DVECTOR* unused_screen_coords) {
    SVECTOR position;
    SVECTOR zoom = g_battle_status_bubble_zoom;
    VECTOR transformed;
    SVECTOR base;
    SVECTOR raised;
    long flag;
    battle_gfx_sprite_display_data_t* display;
    s32 bubble_id;
    /* One local for both digit indices: the status-CT arm reuses the dying
     * counter's register, and two separate locals allocate differently. */
    s32 table_index;
    u8 row;
    u8 flag_row;
    u8 default_row;
    s32 depth;
    s16 y;
    u32* otag;

    /* The caller supplies its culling coordinates in a1, but this routine
     * independently projects the bubble and never reads them. */
    (void)unused_screen_coords;

    bubble_id = unit->status_bubble_id;
    display = unit->status_bubble_display;
    if (bubble_id == 21) {
        display->parts[0].u = 0x72;
        display->parts[0].v = 0xB0;
        if (unit->status_bubble_alternate_row != 0) {
            display->parts[0].v = 0xBC;
        }
    } else if (bubble_id == 0) {
        table_index = unit->stats->death_counter;
        if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_RERAISE) {
            display->parts[0].u = g_battle_status_bubble_u[10];
            flag_row = g_battle_status_bubble_v[10];
            display->parts[0].v = flag_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = flag_row + 12;
            }
        } else if (table_index != 0xFF) {
            display->parts[0].u = g_battle_status_bubble_digit_u[table_index];
            display->parts[0].v = g_battle_status_bubble_digit_v[table_index];
        } else {
            display->parts[0].u = g_battle_status_bubble_u[0];
            default_row = g_battle_status_bubble_v[0];
            display->parts[0].v = default_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = default_row + 12;
            }
        }
    } else if (bubble_id == 20) {
        table_index = unit->stats->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] & 3;
        display->parts[0].u = g_battle_status_bubble_digit_u[table_index];
        display->parts[0].v = g_battle_status_bubble_digit_v[table_index];
    } else {
        display->parts[0].u = g_battle_status_bubble_u[bubble_id];
        row = g_battle_status_bubble_v[bubble_id];
        display->parts[0].v = row;
        if (bubble_id == 9 || bubble_id == 10) {
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = row + 12;
            }
        }
    }

    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    main_util_set_svector(&base, unit->world_coords.vx + unit->event_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->status_bubble_y,
        unit->world_coords.vz + unit->event_offset.vz);
    RotTrans(&base, &transformed, &unit->gte_flag);
    position.vx = unit->status_bubble_x + (transformed.vx + unit->screen_offset.vx);
    y = transformed.vy + unit->screen_offset.vy;
    position.vy = y;
    if (unit->status_bubble_timer & 0x10) {
        position.vy = y - 1;
    }
    battle_camera_convert_screen_coords_modify_by_1(&base, &raised);
    raised.vy += 12;
    RotTrans(&raised, &transformed, &flag);
    display = unit->status_bubble_display;
    otag = main_gfx_get_otag();
    depth = transformed.vz / 4;
    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, 0, 0, &zoom, otag + depth);
}

/* Per-frame float and status-bubble update of one unit: adjusts its float height
 * except during the four float-jump step phases, then advances its status bubble
 * when one is active. */
void battle_gfx_update_float_height_and_status_bubble(battle_unit_t* unit) {
    u8 phase = unit->step_phase;

    if (phase != MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH && phase != MOVE_STEP_PHASE_FLOAT_JUMP_EAST
        && phase != MOVE_STEP_PHASE_FLOAT_JUMP_WEST && phase != MOVE_STEP_PHASE_FLOAT_JUMP_NORTH) {
        battle_unit_increment_or_decrement_height_mod(unit);
    }
    if (unit->status_bubble_active != 0) {
        battle_gfx_determine_status_bubble_parameters(unit);
    }
}

/* Start the unit's afterimage trail: enable position_copies and seed all six at its world
 * position. Jump launches and SEQ opcode 0xbf start it. */
void battle_gfx_init_position_vector_copies(battle_unit_t* unit) {
    s32 i;

    unit->position_copies_active = 1;
    for (i = 0; i < 6; i++) {
        main_util_set_svector(
            &unit->position_copies[i], unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    }
}

/*
 * Draw the unit's current sprite frame once at each of its six position_copies:
 * the afterimage trail that battle_gfx_init_position_vector_copies starts.
 *
 * Each copy is projected with the battle camera and queued at its own
 * ordering-table depth using the current camera zoom, semi-transparent with the
 * blend bits cleared (half blend). The camera matrices are rebuilt for every
 * copy.
 */
void battle_gfx_draw_unit_sprite_position_copies(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    VECTOR transformed;
    long flag;
    battle_gfx_sprite_display_data_t* display;
    s32 depth;
    s32 i;
    s16 flags;

    for (i = 0; i < 6; i++) {
        battle_camera_update_matrices(&g_battle_camera_matrix, &g_battle_camera_render_state,
            &g_battle_camera_current_screen_offset, &g_battle_camera_zoom);
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        RotTrans(&unit->position_copies[i], &transformed, &flag);
        depth = transformed.vz / 4;
        position.vx = transformed.vx;
        position.vy = transformed.vy;
        flags = ((unit->horizontal_flip_flag ^ unit->sprite_display_flags) & ~BATTLE_SPRITE_DISPLAY_BLEND_MASK)
            | BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
        copyVector(&zoom, &g_battle_camera_zoom);
        display = unit->animation.display;
        battle_gfx_construct_polygon_data_for_units(
            display, display->part_count, 0, &position, 0, flags, &zoom, main_gfx_get_otag() + depth);
    }
}

/* Advances the unit's position trail: each copy takes the next newer one and copy
 * 0 the unit's current position. */
void battle_gfx_shift_and_fill_position_copies(battle_unit_t* unit) {
    s32 index;

    for (index = 5; index > 0; index--) {
        unit->position_copies[index] = unit->position_copies[index - 1];
    }
    battle_unit_copy_draw_coords(unit->position_copies, unit);
}

/* Make the unit's item display show an item's 16x16 icon, centered on the
 * display's origin. */
void battle_gfx_store_item_display_data(battle_unit_t* unit, u32 item_id) {
    SPRT data;

    gfx_set_item_icon_sprite(&data, item_id);
    unit->item_display->clut = data.clut;
    battle_gfx_store_sprite_display_data(
        unit->item_display, 0, (16 - data.w) / 2 - 8, (16 - data.h) / 2 - 8, data.u0, data.v0, data.w, data.h, 0);
}

/* Queues the unit's item icon at its item-get offset from the unit's screen
 * position, at the unit's ordering-table depth and scale ONE. The x offset is
 * negated unless the sprite is flipped horizontally. */
void battle_gfx_init_item_get_rendering(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    u32* otag;

    if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
        position.vx = unit->item_get_x_offset;
    } else {
        position.vx = -unit->item_get_x_offset;
    }
    position.vy = unit->item_get_y_offset;
    position.vx += unit->camera_relative_x;
    position.vy += unit->camera_relative_y;
    main_util_set_svector(&zoom, ONE, ONE, ONE);
    otag = main_gfx_get_otag();
    otag += unit->otag_depth_index;
    battle_gfx_construct_polygon_data_for_units(
        unit->item_display, unit->item_display->part_count, 0, &position, 0, 0, &zoom, otag);
}

/* Load the unit's next pending action result into its numeric display and clear its pending flag:
 * a stat change, then level down or up, stolen, broken, Quick, CT 0, missed, guarded, caught, then
 * the queued status images. Sets the digit sprites (values over 999 show 999) and the color.
 * Returns 0 when nothing is pending. */
s32 battle_gfx_build_next_action_result_display(battle_unit_t* unit) {
    unit_stats_t* stats;
    u32 scan_flags;
    s32 flags;
    action_result_display_t* display;
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    const action_display_image_rect_t* image;
    s32 flag;
    s32 index;
    s32 digits[3];
    u8 red;
    u8 green;
    u8 blue;
    s16 y_shift;
    s32 image_id;
    u8 change;
    u8 status;

    flags = unit->action_display_flags;
    stats = unit->stats;
    display = &unit->numeric_display;

    if ((flags & ACTION_DISPLAY_NUMERIC_FLAGS_MASK) != 0) {
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_HP_DAMAGE] = stats->action.hp_damage;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_HP_HEALING] = stats->action.hp_healing;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_MP_DAMAGE] = stats->action.mp_damage;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_MP_HEALING] = stats->action.mp_healing;
        /* A stat's gain and damage slots share one magnitude; the pending flag picks which shows. */
        change = stats->action.sp_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_SPEED_GAIN] = change;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE] = change;
        change = stats->action.ct_change & 0x7f;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_CT_GAIN] = change;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_CT_DAMAGE] = change;
        change = stats->action.brave_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN] = change;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE] = change;
        change = stats->action.faith_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_FAITH_GAIN] = change;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE] = change;
        flag = 1;
        change = stats->action.pa_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_PA_GAIN] = change;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_PA_DAMAGE] = change;
        index = 0;
        change = stats->action.ma_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_MA_GAIN] = change;
        g_battle_action_display_values[BATTLE_NUMERIC_DISPLAY_MA_DAMAGE] = change;

        /* Show the lowest pending numeric result and clear its flag. */
        scan_flags = unit->action_display_flags;
        while (index < ACTION_DISPLAY_VALUE_COUNT) {
            if ((flag & scan_flags) != 0) {
                flags = index;
                display->value = g_battle_action_display_values[flags];
                display->selector = g_battle_action_display_numeric_selectors[flags];
                index = unit->action_display_flags;
                unit->action_display_flags = ~flag & index;
                break;
            }
            index++;
            flag <<= 1;
        }
    } else if ((flags & ACTION_DISPLAY_FLAG_LEVEL_DOWN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_LEVEL_DOWN;
    } else if ((flags & ACTION_DISPLAY_FLAG_LEVEL_UP) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_LEVEL_UP;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_LEVEL_UP;
    } else if ((flags & ACTION_DISPLAY_FLAG_STOLEN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_STOLEN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_STOLEN;
    } else if ((flags & ACTION_DISPLAY_FLAG_BROKEN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_BROKEN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_BROKEN;
    } else if ((flags & ACTION_DISPLAY_FLAG_QUICK) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_QUICK;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_QUICK;
    } else if ((flags & ACTION_DISPLAY_FLAG_CT_ZERO) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_CT_ZERO;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_CT_ZERO;
    } else if ((flags & ACTION_DISPLAY_FLAG_MISSED) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_MISSED;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_MISSED;
    } else if ((flags & ACTION_DISPLAY_FLAG_GUARDED) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_GUARDED;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_GUARDED;
    } else if ((flags & ACTION_DISPLAY_FLAG_CAUGHT) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_CAUGHT;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_CAUGHT;
    } else if (unit->status_display_image_count != 0) {
        unit->status_display_image_count--;
        status = unit->status_display_image_list[unit->status_display_image_count];
        if ((status & ACTION_STATUS_DISPLAY_REMOVAL) != 0) {
            display->selector = BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED;
        } else {
            display->selector = BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED;
        }
        status &= ACTION_STATUS_DISPLAY_IMAGE_MASK;
    } else {
        return 0;
    }

    if (unit->stats->action.critical != 0) {
        display->selector |= BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL;
    } else {
        display->selector &= ~BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL;
    }

    display->active = 1;
    if (display->progress >= 22) {
        display->progress = 21;
    }

    /* Every message produced above selects a color case.  The target has no
     * default initialization for these three registers. */
    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED:
    case BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN:
    case BATTLE_NUMERIC_DISPLAY_STOLEN:
    case BATTLE_NUMERIC_DISPLAY_BROKEN:
    case BATTLE_NUMERIC_DISPLAY_QUICK:
    case BATTLE_NUMERIC_DISPLAY_CT_ZERO:
        blue = 128;
        green = 128;
        red = 128;
        break;
    case BATTLE_NUMERIC_DISPLAY_MP_DAMAGE:
        red = 120;
        green = 60;
        blue = 220;
        break;
    case BATTLE_NUMERIC_DISPLAY_HP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_SPEED_GAIN:
    case BATTLE_NUMERIC_DISPLAY_CT_GAIN:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN:
    case BATTLE_NUMERIC_DISPLAY_FAITH_GAIN:
    case BATTLE_NUMERIC_DISPLAY_PA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_MA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED:
    case BATTLE_NUMERIC_DISPLAY_LEVEL_UP:
        red = 40;
        green = 140;
        blue = 220;
        break;
    case BATTLE_NUMERIC_DISPLAY_MP_HEALING:
        red = 78;
        green = 140;
        blue = 30;
        break;
    case BATTLE_NUMERIC_DISPLAY_MISSED:
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
    case BATTLE_NUMERIC_DISPLAY_CAUGHT:
        red = 190;
        green = 40;
        blue = 10;
        break;
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = red;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = green;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = blue;

    if (display->value / 1000) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = display->value % 10;
        digits[1] = (display->value / 10) % 10;
        digits[2] = display->value / 100;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    /* Empty displays 0 and 1; each case below fills only the sprites it uses. */
    battle_gfx_store_sprite_display_data(display_0, 0, 0, -32, 0, 0, 0, 0, 0);
    battle_gfx_store_sprite_display_data(display_1, 0, 0, -32, 0, 0, 0, 0, 0);

    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_HP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_MP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MP_HEALING:
        /* Damage (even selectors) shows at y 16, gains and healing at y -32. */
        y_shift = -32;
        if ((display->selector & 1) == 0) {
            y_shift = 16;
        }
        battle_gfx_store_sprite_display_data(display_1, 0, -14, y_shift, 0, 0, 0, 0, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, -21, y_shift, 0, 0, 0, 0, 0);
        if ((digits[2] | digits[1]) == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -21, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else if (digits[2] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_2, 0, -21, y_shift, 0xa8 + digits[2] * 8, 0x30, 8, 16, 0);
        }
        break;

    case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_SPEED_GAIN:
    case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_CT_GAIN:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN:
    case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_FAITH_GAIN:
    case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_PA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MA_GAIN:
        y_shift = -32;
        if ((display->selector & 1) == 0) {
            y_shift = 16;
        }
        if (digits[1] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }

        /* Masking bit 0 lets each damage case also label the matching gain. */
        switch (display->selector & 0x0e) {
        case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x9c, 0xc2, 0x18, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0xc2, 0xc2, 0x0e, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0xa8, 0xb8, 0x0b, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x9c, 0xb8, 0x0c, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x40, 0, 0x0c, 0x0d, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x40, 0x0d, 0x0b, 0x0a, 0);
            break;
        }
        break;

    case BATTLE_NUMERIC_DISPLAY_MISSED:
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xbe, 0x48, 0x1b, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xb8, 0x60, 0x1e, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_CAUGHT:
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0x30, 0xe6, 0x1a, 0x0a, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED:
    case BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED:
        image_id = status;
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, g_battle_action_display_image_rects[image_id].u,
            g_battle_action_display_image_rects[image_id].v, g_battle_action_display_image_rects[image_id].width,
            (g_battle_action_display_image_rects + image_id)->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN:
        /* The fall-through overwrites both sprites, so level down places them like level up. */
        battle_gfx_store_sprite_display_data(display_1, 0, -7, 16, 0xb0, 0x30, 8, 16, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, 0, 16, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        /* fall through */
    case BATTLE_NUMERIC_DISPLAY_LEVEL_UP:
        battle_gfx_store_sprite_display_data(display_1, 0, -7, -32, 0xb0, 0x30, 8, 16, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, 0, -32, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_STOLEN:
        image = &g_battle_action_display_image_rects[ACTION_DISPLAY_IMAGE_STOLEN];
        battle_gfx_store_sprite_display_data(
            display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_BROKEN:
        image = &g_battle_action_display_image_rects[ACTION_DISPLAY_IMAGE_BROKEN];
        battle_gfx_store_sprite_display_data(
            display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_QUICK:
        image = &g_battle_action_display_image_rects[ACTION_DISPLAY_IMAGE_QUICK];
        battle_gfx_store_sprite_display_data(
            display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_CT_ZERO:
        /* y_shift is never set on this path; its stale value picks the row (QUIRKS.md). */
        battle_gfx_store_sprite_display_data(display_1, 0, -16, y_shift, 0xc2, 0xc2, 0x0e, 8, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, 0, y_shift, 0xa8, 0x34, 8, 8, 0);
        break;
    }

    battle_map_load_palette_data(&g_battle_action_result_palette, MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0);
    return 1;
}

/* Queues the post-action displays for a unit's resolved action.
 *
 * A hit sets one action_display_flags bit per HP/MP, stat, level, steal and
 * break result and lists the inflicted/removed status images (removals
 * marked 0x80); a miss queues MISSED, GUARDED or CAUGHT by miss type. The
 * first display is built immediately unless a numeric display is active. */
void battle_gfx_prepare_post_action_display(battle_unit_t* unit) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 i;
    s32 j;
    s32 bit;
    u8 count;
    u8 image;
    u8 miss;

    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = BATTLE_GFX_TPAGE_FRAME;
    stats = unit->stats;
    action = &stats->action;
    if (action->hit != 0) {
        if (action->attack_type & ACTION_TYPE_HP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_HP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_MP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_MP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_PSEUDO_STATUS) {
            if (action->sp_change != 0) {
                if (action->sp_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_DAMAGE;
                }
            }
            if (action->ct_change != 0) {
                if (action->ct_change == ACTION_CT_CHANGE_QUICK) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_QUICK;
                } else if (action->ct_change == ACTION_CT_CHANGE_ZERO) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_ZERO;
                } else if (action->ct_change & 0x80) { /* the top bit marks a gain, as for the stats */
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_DAMAGE;
                }
            }
            if (action->pa_change != 0) {
                if (action->pa_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_DAMAGE;
                }
            }
            if (action->ma_change != 0) {
                if (action->ma_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_DAMAGE;
                }
            }
            if (action->brave_change != 0) {
                if (action->brave_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_DAMAGE;
                }
            }
            if (action->faith_change != 0) {
                if (action->faith_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_DAMAGE;
                }
            }
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_UP;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_DOWN;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_STOLEN;
        }
        if (action->special_effect
            & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_BROKEN;
        }
        if (action->attack_type & ACTION_TYPE_STATUS_CHANGE) {
            unit->status_display_image_count = 0;
            for (i = 0; i < STATUS_BYTE_COUNT; i++) {
                for (j = 0; j < 8; j++) {
                    count = unit->status_display_image_count;
                    bit = 0x80 >> j;
                    if (count < ACTION_STATUS_DISPLAY_CAPACITY) {
                        if (action->status_changes.infliction[i] & bit) {
                            image = g_battle_action_status_display_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_count++;
                            }
                        } else if (action->status_changes.removal[i] & bit) {
                            image = g_battle_action_status_display_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_list[unit->status_display_image_count]
                                    |= ACTION_STATUS_DISPLAY_REMOVAL;
                                unit->status_display_image_count++;
                            }
                        }
                    }
                }
            }
        }
    } else {
        /* An evade (accessory, either hand, Blade Grasp or Finger Guard) or a forced
         * failure reads MISSED for an ability that targets allies only (team bits 1,
         * AI_ABILITY_FLAG_1_TARGET_ALLIES) and GUARDED otherwise; a catch reads
         * CAUGHT, a canceled or reflected action nothing, any other miss MISSED. */
        miss = action->miss_type;
        if (miss != ACTION_MISS_TYPE_HIT) {
            /* types 1 and 2: accessory and right-hand evade */
            if ((u32)(action->miss_type - 1) < 2 || miss == ACTION_MISS_TYPE_LEFT_HAND_EVADE
                || miss == ACTION_MISS_TYPE_FORCED_FAILURE || miss == ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD) {
                if ((action->miss_display_flags & ACTION_MISS_DISPLAY_TEAM_MASK) == 1) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_GUARDED;
                }
            } else if (miss == ACTION_MISS_TYPE_CATCH) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_CAUGHT;
            } else if (miss != ACTION_MISS_TYPE_CANCELED && miss != ACTION_MISS_TYPE_HIT
                && miss != ACTION_MISS_TYPE_REFLECTED) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
            }
        }
    }
    if (unit->numeric_display.active == 0) {
        unit->numeric_display.progress = 0;
        battle_gfx_build_next_action_result_display(unit);
    }
}

/* Builds the next EXP/JP or failure-message display: gained Exp, then gained JP,
 * then no target, silenced or not enough MP, clearing that flag. A number shows
 * its last two digits (a value over 999 shows 99).
 *
 * The companion of battle_gfx_build_next_action_result_display for flags
 * 0x02000000 and above. Returns 0 when no flag is pending. Reading the
 * flags into a block-local before clearing keeps the target's and operand
 * order. */
s32 battle_gfx_build_next_special_action_result_display(battle_unit_t* unit) {
    action_result_display_t* display;
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    s32 digits[3];
    u32 flags;
    u32 clear;
    u16 selector;

    flags = unit->action_display_flags;
    display = &unit->numeric_display;
    if ((flags & ACTION_DISPLAY_FLAG_GAINED_EXP) != 0) {
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_GAINED_EXP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_experience;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_EXP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else if ((flags & ACTION_DISPLAY_FLAG_GAINED_JP) != 0) {
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_GAINED_JP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_jp;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_JP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else {
        if ((flags & ACTION_DISPLAY_FLAG_NO_TARGET) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_NO_TARGET;
            selector = BATTLE_NUMERIC_DISPLAY_NO_TARGET;
        } else if ((flags & ACTION_DISPLAY_FLAG_SILENCED) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_SILENCED;
            selector = BATTLE_NUMERIC_DISPLAY_SILENCED;
        } else if ((flags & ACTION_DISPLAY_FLAG_NO_MP) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_NO_MP;
            selector = BATTLE_NUMERIC_DISPLAY_NO_MP;
        } else {
            return 0;
        }
        unit->numeric_display.selector = selector;
        unit->numeric_display.active = 1;
        {
            u32 current;
            current = unit->action_display_flags;
            unit->action_display_flags = current & clear;
        }
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = 150;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = 140;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = 20;

    if ((display->value / 1000) & 0xffff) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = (display->value % 10) & 0xffff;
        digits[1] = (((display->value / 10) & 0xffff) % 10) & 0xffff;
        digits[2] = (display->value / 100) & 0xffff;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    /* Empty displays 1 and 2; each case below fills only the sprites it uses. */
    battle_gfx_store_sprite_display_data(display_2, 0, -21, 16, 0, 0, 0, 0, 0);
    battle_gfx_store_sprite_display_data(display_1, 0, -21, 16, 0, 0, 0, 0, 0);

    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
        if (digits[1] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
        if (digits[1] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case BATTLE_NUMERIC_DISPLAY_NO_TARGET:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        battle_gfx_store_sprite_display_data(display_0, 0, -14, -24, 0x1a, 0xdb, 0x25, 10, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_SILENCED:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        battle_gfx_store_sprite_display_data(display_0, 0, -14, -24, 0xd8, 0xe9, 0x1e, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_NO_MP:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        battle_gfx_store_sprite_display_data(display_0, 0, -14, -24, 0x64, 0x20, 0x1a, 9, 0);
        break;
    }

    battle_map_load_palette_data(&g_battle_action_result_palette, MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0);
    return 1;
}

/* Queues the gained Exp and JP displays after an action, when the Display Gained
 * Exp/JP option is on and the amount is not 0. The first display is built at once
 * unless one is already running. */
void battle_gfx_init_earned_exp_jp_display(battle_unit_t* unit) {
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;

    if (g_main_game_options.fields.display_gained_exp_jp == GAME_OPTION_ON) {
        display_0 = unit->numeric_display.displays[0];
        display_1 = unit->numeric_display.displays[1];
        display_2 = unit->numeric_display.displays[2];
        display_2->tpage = BATTLE_GFX_TPAGE_FRAME;
        display_1->tpage = BATTLE_GFX_TPAGE_FRAME;
        display_0->tpage = BATTLE_GFX_TPAGE_FRAME;
        if (unit->strike.action_rewards.earned_experience != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_EXP;
        }
        if (unit->strike.action_rewards.earned_jp != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_JP;
        }
        if (unit->numeric_display.active == 0) {
            unit->numeric_display.progress = 0;
            battle_gfx_build_next_special_action_result_display(unit);
        }
    }
}

/* Queues the failure message for a charged action that cannot start: mode is the
 * action_rejection_e of battle_action_init_current_data, and only no target,
 * silenced and not enough MP queue one.
 *
 * The three numeric displays are pointed at the FRAME.BIN page, and the first
 * display is built at once unless one is already running. */
void battle_gfx_queue_charge_failure_message(battle_unit_t* unit, s32 mode) {
    battle_gfx_sprite_display_data_t* display_0 = unit->numeric_display.displays[0];
    battle_gfx_sprite_display_data_t* display_1 = unit->numeric_display.displays[1];
    battle_gfx_sprite_display_data_t* display_2 = unit->numeric_display.displays[2];

    display_2->tpage = BATTLE_GFX_TPAGE_FRAME;
    display_1->tpage = BATTLE_GFX_TPAGE_FRAME;
    display_0->tpage = BATTLE_GFX_TPAGE_FRAME;
    switch (mode) {
    case ACTION_REJECTION_NO_TARGET:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_TARGET;
        break;
    case ACTION_REJECTION_SILENCED:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_SILENCED;
        break;
    case ACTION_REJECTION_NOT_ENOUGH_MP:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_MP;
        break;
    }
    if (unit->numeric_display.active == 0) {
        unit->numeric_display.progress = 0;
        battle_gfx_build_next_special_action_result_display(unit);
    }
}
