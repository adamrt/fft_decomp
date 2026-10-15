/* Gfx cursor and unit sprites. Its start is a cut chosen by topic; its end is an object boundary the read-only data
 * layout places somewhere nearby. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/libgpu.h"
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
s32 battle_target_calculate_cursor_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad) {
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    s32 split;
    s32 rise;
    u8 height;
    u8 depth;
    u8 slope;

    split = 0;
    tile = battle_map_get_tile_data_pointer((coords->vx / MAP_TILE_SIZE), (coords->vz / MAP_TILE_SIZE), layer);
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

s32 battle_move_calculate_water_lift(battle_unit_t* unit, map_tile_t* tile) {
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
battle_unit_t* battle_unit_get_mount(battle_unit_t* unit) {
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        return battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
    }
    return unit;
}

/*
 * Returns the signed-16-bit world height at coords after applying unit height,
 * terrain depth, movement behavior and sprite-specific adjustments.
 * vx/vz are integer world x/depth; vy selects the map layer.
 * An unavailable unit-height record falls back to terrain height.
 */
s32 battle_gfx_calculate_world_height_from_coords(battle_unit_t* unit, const SVECTOR* coords) {
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
            world_height = -height.total_height / 2 * 12;
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
                world_height -= (tile->depth_half_height.depth) * 12 + unit->depth_height_offset;
            } else if (sprite == BATTLE_SPRITESHEET_ID_CHICKEN || sprite == BATTLE_SPRITESHEET_ID_FROG) {
                raised = world_height + 4;
                world_height = raised - ((tile->depth_half_height.depth) * 12 + unit->depth_height_offset);
            } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) || animation == BATTLE_UNIT_ANIMATION_DEAD) {
                world_height -= ((tile->depth_half_height.depth) - 1) * 12 + unit->depth_height_offset;
            } else if (sprite == BATTLE_SPRITESHEET_ID_TREASURE || sprite == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                world_height -= (tile->depth_half_height.depth) * 12 + unit->depth_height_offset;
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
 * rider's result is moved by -10 plus the mount's float bob offset. */
s32 battle_gfx_calculate_world_height_with_caller_coords(battle_unit_t* unit, const SVECTOR* coords) {
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
    world_height = battle_gfx_calculate_world_height_from_coords(mount, coords);
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
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
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
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
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
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
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

/*
 * Calculate world height at the grounded unit's map-tile center.
 *
 * A mounted rider uses the mount's map position, then applies its mounted
 * height offset and the original -10 adjustment.
 */
s32 battle_gfx_calculate_world_height_from_battle_tile(battle_unit_t* unit) {
    SVECTOR coord;
    unit_stats_t* stats;
    battle_unit_t* mount;
    s32 adjusted_world_height;
    s32 world_height;

    mount = battle_unit_get_mount(unit);
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
 * The four shadow corners at 0x29c are projected into a semi-transparent
 * POLY_FT4. On a tile with depth where screen_z is greater than
 * -(height + depth) * 12, the quad uses tpage 0x3f and UVs that rotate with
 * the counter at 0x29a; otherwise it uses tpage 0x5f and fixed UVs.
 * When the unit is above the ground height at its effect-adjusted position,
 * the quad is sorted by that ground point's depth; otherwise it goes into
 * the caller's ordering-table entry.
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
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    prim = g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter();
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
    if (tile->depth_half_height.depth && -(tile->height + tile->depth_half_height.depth) * 12 < unit->world_coords.vy) {
        unit->shadow_counter += g_main_system_animation_speed;
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
        position.vy -= tile->depth_half_height.depth * 12;
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
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&offset, &transformed, &flag);
        depth = transformed.vz / 4;
        packet = main_gfx_get_otag() + depth;
    }
    AddPrim(packet, prim);
}

/*
 * Queue the glow primitives for one cursor tile.
 *
 * The four vertices are projected with the battle camera. Shape 0 draws a textured
 * quad; shapes 1 and 2 split the tile into two textured triangles along either
 * diagonal. Each primitive gets a copy with the 0x1f texture page and the
 * 0x50 palette, and both are queued only when the first three projected
 * vertices face the camera. The pool stride is always sizeof(POLY_FT4).
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

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    switch (shape) {
    case 0:
        quad = g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter();
        quad_glow = g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter();
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
        tri_a = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
        tri_b = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
        tri_a_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
        tri_b_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
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
        tri_a = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
        tri_b = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
        tri_a_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
        tri_b_glow = (POLY_FT3*)(g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter());
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

/* Per-sprite height table, one byte every four. */

/*
 * Queue the map selection cursor over the currently selected tile.
 *
 * The explicit default arm is required: it gives GCC the target's jump past
 * the default 0x3c path. Scheduling the r0 color assignment directly after
 * u2 retains the primitive's semantics and produces the nearer late move.
 *
 * The two initialized locals occupy 64 bytes of .rodata at 0x80067798: the
 * eight-frame vertical bob offsets and their per-frame durations. The bob
 * advances once per call unless the status screen owns the frame. The cursor
 * floats 0x3c above the tile, or above the occupying unit's sprite height
 * scaled by the camera zoom, and its quad is sized from the camera zoom.
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
    s32 scaled;
    s32 half_width;
    s32 half_height;
    u8 color;

    prim = g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter();
    ((P_TAG*)prim)->len = 9;
    prim->code = 0x2c;
    if (g_battle_menu_status_screen_selected != 1) {
        g_battle_gfx_selection_cursor_bob_timer += g_main_system_animation_speed;
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
        scaled = g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height * g_camera_zoom.vy;
        height = (scaled / ONE) + 0x18;
    } else {
        height = 0x3c;
    }
    base->vy -= height - bob[g_battle_gfx_selection_cursor_bob_frame];
    RotTrans(base, &transformed, &flag);

    half_width = g_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x0 = transformed.vx - (half_width >> 9);
    {
        s32 zoom_height = g_camera_zoom.vy * 12;
        prim->y0 = transformed.vy - (zoom_height / ONE);
    }
    half_width = g_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x1 = transformed.vx + (half_width >> 9);
    {
        s32 zoom_height = g_camera_zoom.vy * 12;
        prim->y1 = transformed.vy - (zoom_height / ONE);
    }
    half_width = g_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x2 = transformed.vx - (half_width >> 9);
    {
        s32 zoom_height = g_camera_zoom.vy * 12;
        prim->y2 = transformed.vy + (zoom_height / ONE);
    }
    half_width = g_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x3 = transformed.vx + (half_width >> 9);
    half_height = g_camera_zoom.vy * 12;
    if (half_height < 0) {
        half_height += 0xfff;
    }
    {
        u16 screen_y = transformed.vy;
        prim->tpage = 0x5f;
        prim->y3 = screen_y + (half_height >> 12);
    }
    prim->code |= 2;
    color = 0x80;
    prim->clut = GetClut(0x40, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
    prim->r0 = prim->g0 = prim->b0 = color;
    prim->u2 = 0x40;
    prim->u0 = 0x40;
    prim->u3 = 0x50;
    prim->u1 = 0x50;
    prim->v1 = color;
    prim->v0 = color;
    prim->v3 = 0x98;
    prim->v2 = 0x98;
    battle_camera_convert_screen_coords_modify_by_1(base, &screen);
    RotTrans(&screen, &transformed, &flag);
    AddPrim(ot + (transformed.vz / 4), prim);
}

/*
 * Queue the four wait-direction arrows around a unit's feet, highlighting the
 * one that matches its facing quadrant.  Called from
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
    g_battle_gfx_wait_arrow_blink_timer += g_main_system_animation_speed;
    for (i = 0; i < 4; i++) {
        prim = g_battle_gfx_sprite_prim_pool + battle_gfx_increment_counter();
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
