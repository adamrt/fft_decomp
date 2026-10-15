#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Transformed model points live in the scratchpad as VECTORs, with the GTE
 * flag of each transform stored in `pad`. */
/* Byte addressing preserves the original addition operand order. */
#define PT(k) (*(VECTOR*)((u8*)scratch + (command.vertices.indices[k] << 4)))

/* Draw one of the projectile models resident in BATTLE.BIN for the current
 * secondary effect: the low byte of parameter is a effect_model_e.
 *
 * The model's vertices go through three transforms in the scratchpad: the spin
 * rotation, then the angles rotation with scale and position, then the camera
 * matrix. The model's draw commands are then decoded one by one into the effect's
 * work buffer for the current frame buffer and linked into the effect ordering
 * table:
 * - ARROW: G4 and G3 commands, at a quarter of the corners' average z (their sum
 *   over 16 or 12).
 * - STONE and SHURIKEN: G3 commands only, at the same depth.
 * - REFLECT: only the first timer * 3 commands, so the shield grows by three
 *   triangles a frame. Its GT3 commands use texture page 0x3f and the CLUT at
 *   0x7eff plus the effect's own slot id. The depth is the last corner's z over
 *   12, and every triangle is linked at ordering table entry 0, not at its
 *   depth.
 *
 * A polygon whose depth is not between 1 and EFFECT_OT_DEPTH_MAX - 1 is built but
 * not drawn. */
void effect_draw_projectile_model(SVECTOR* spin, VECTOR* position, SVECTOR* angles, VECTOR* scale, s32 parameter) {
    effect_decoded_model_command_t command;
    s32 unused[2];
    MATRIX matrix;
    SVECTOR point;
    psyq_tmd_header_t* model;
    SVECTOR* vertices;
    VECTOR* scratch;
    s32 count;
    s32 i;
    s32 command_count;
    s32 gt3_index;
    s32 g4_index;
    s32 g3_index;
    s32 j;
    s32 z;
    u32* ot;

    RotMatrix(spin, &matrix);
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    model = g_effect_model_files[parameter & 0xff];
    scratch = (VECTOR*)getScratchAddr(0);
    vertices = (SVECTOR*)((u8*)model->objects + model->objects[0].vertex_offset);
    count = model->objects[0].vertex_count;
    for (i = 0; i < count; i++) {
        gte_ldv0(&vertices[i]);
        gte_rtv0tr();
        gte_stlvnl(&scratch[i]);
        gte_stflg(&scratch[i].pad);
    }
    RotMatrix(angles, &matrix);
    ScaleMatrixL(&matrix, scale);
    matrix.t[0] = position->vx;
    matrix.t[1] = position->vy;
    matrix.t[2] = position->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    for (i = 0; i < count; i++) {
        copyVector(&point, &scratch[i]);
        gte_ldv0(&point);
        gte_rtv0tr();
        gte_stlvnl(&scratch[i]);
        gte_stflg(&scratch[i].pad);
    }
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    for (i = 0; i < count; i++) {
        copyVector(&point, &scratch[i]);
        gte_ldv0(&point);
        gte_rtv0tr();
        gte_stlvnl(&scratch[i]);
        gte_stflg(&scratch[i].pad);
    }
    command_count = effect_init_model_sections(model, 0);
    gt3_index = 0;
    g4_index = 0;
    g3_index = 0;
    switch (parameter & 0xff) {
    case EFFECT_MODEL_ARROW: {
        effect_arrow_buffer_t* work;
        POLY_G4* quad;
        POLY_G3* tri;

        work = g_effect_current_secondary->allocation;
        for (i = 0; i < command_count;) {
            effect_decode_next_model_command(&command);
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G4) {
                quad = &work->g4[g_effect_buffer_index][g4_index];
                setRGB0(quad, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(quad, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(quad, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                setRGB3(quad, command.colors[3].r, command.colors[3].g, command.colors[3].b);
                quad->x0 = PT(0).vx;
                quad->y0 = PT(0).vy;
                z = PT(0).vz;
                quad->x1 = PT(1).vx;
                quad->y1 = PT(1).vy;
                z += PT(1).vz;
                quad->x2 = PT(2).vx;
                quad->y2 = PT(2).vy;
                z += PT(2).vz;
                quad->x3 = PT(3).vx;
                quad->y3 = PT(3).vy;
                g4_index++;
                z += PT(3).vz;
                z /= 16;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, quad);
                }
            }
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][g3_index];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                g3_index++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_STONE: {
        effect_stone_buffer_t* work;
        POLY_G3* tri;

        work = g_effect_current_secondary->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            effect_decode_next_model_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][j];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                j++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_REFLECT: {
        effect_reflect_buffer_t* work;
        POLY_GT3* tri;

        work = g_effect_current_secondary->allocation;
        /* g4_index is reused here as the limit on decoded commands. */
        g4_index = g_effect_current_secondary->timer * 3;
        if (command_count < g4_index) {
            g4_index = command_count;
        }
        for (i = 0; i < g4_index;) {
            effect_decode_next_model_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_GT3_37) {
                tri = &work->prims[g_effect_buffer_index][gt3_index];
                tri->tpage = 0x3f;
                tri->clut = g_effect_current_secondary->own_slot_id + 0x7eff;
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                memcpy(&tri->u0, &command.texture.uv[0], sizeof(command.texture.uv[0]));
                memcpy(&tri->u1, &command.texture.uv[1], sizeof(command.texture.uv[1]));
                memcpy(&tri->u2, &command.texture.uv[2], sizeof(command.texture.uv[2]));
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z = PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z = PT(2).vz;
                z /= 12;
                gt3_index++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    ot = g_effect_polygon_depth_data;
                    addPrim(ot, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_SHURIKEN: {
        effect_shuriken_buffer_t* work;
        POLY_G3* tri;

        work = g_effect_current_secondary->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            effect_decode_next_model_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][j];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                j++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    }
}

/*
 * Queues the item-drop sprite for the current secondary effect: projects the
 * effect origin to find its OT depth, builds a rotated and camera-scaled 2D
 * matrix from the sprite angle, and transforms the four quad corners into the
 * slot's double-buffered primitive before adding it to the ordering table.
 */
void effect_draw_item_drop_sprite(VECTOR* origin, s32 item_id, s32 angle) {
    SPRT sprite;
    SVECTOR local;
    VECTOR position;
    s32 sxy;
    s32 flag;
    effect_slot_t* slot;
    POLY_FT4* prim;
    s32 depth;
    s32 sine;
    s32 cosine;
    s32 zoom;
    s32 scale_y;
    s32 half_w;
    s32 half_h;
    s16 right;
    s16 bottom;

    slot = &g_effect_slots[g_effect_current_secondary->slot_ids[0]];
    gfx_set_item_icon_sprite(&sprite, item_id);
    prim = &slot->quads[g_effect_buffer_index];
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    copyVector(&local, origin);
    RotTrans(&local, &position, &position.pad);
    depth = (position.vz >> 2) - 8;
    if (depth < EFFECT_OT_DEPTH_MIN) {
        depth = EFFECT_OT_DEPTH_MIN;
    }
    if (depth >= EFFECT_OT_DEPTH_LIMIT) {
        depth = EFFECT_OT_DEPTH_MAX;
    }
    g_effect_matrix.t[0] = position.vx;
    g_effect_matrix.t[1] = position.vy;
    cosine = rcos(angle);
    sine = rsin(angle);
    zoom = g_camera_zoom.vx;
    scale_y = g_camera_zoom.vy;
    g_effect_matrix.m[0][0] = (cosine * zoom) >> 12;
    g_effect_matrix.m[1][1] = (cosine * scale_y) >> 12;
    g_effect_matrix.m[0][1] = (-sine * scale_y) >> 12;
    g_effect_matrix.m[1][0] = (sine * zoom) >> 12;
    SetRotMatrix(&g_effect_matrix);
    SetTransMatrix(&g_effect_matrix);
    half_w = -sprite.w / 2;
    right = half_w + sprite.w;
    half_h = -sprite.h / 2;
    bottom = half_h + sprite.h;
    g_effect_corner_bottom_left.vx = half_w;
    g_effect_corner_top_left.vx = half_w;
    g_effect_corner_bottom_right.vx = right;
    g_effect_corner_top_right.vx = right;
    g_effect_corner_top_right.vy = half_h;
    g_effect_corner_top_left.vy = half_h;
    g_effect_corner_bottom_right.vy = bottom;
    g_effect_corner_bottom_left.vy = bottom;
    RotTransPers3(&g_effect_corner_top_left, &g_effect_corner_top_right, &g_effect_corner_bottom_left, (s32*)&prim->x0,
        (s32*)&prim->x1, (s32*)&prim->x2, &sxy, &flag);
    RotTransPers(&g_effect_corner_bottom_right, (s32*)&prim->x3, &sxy, &flag);
    AddPrim(g_effect_polygon_depth_data + depth, prim);
}

/* Quadratic height term used by the projectile trajectory calculation;
 * this is not the complete trajectory.
 * distance is the attacker-to-target distance scaled by ONE. */
s32 effect_calculate_arc_height_term(s32 distance) {
    s32 gravity;

    distance >>= 6;
    gravity = g_effect_gravity_modifier;
    return gravity / 2 - (distance * distance) / (gravity >> 11);
}

/* Solve the two launch slopes of an arcing projectile toward the target.
 *
 * Lowers the target's aim height from half, to three quarters, to zero of its
 * sprite height until both the arc apex and the target tile floor clear it,
 * storing the chosen height back into the target endpoint. Returns 0 when the
 * target is at zero distance or no height clears. */
s32 effect_calculate_arcing_trajectory(battle_arc_query_t* query, s32* arc_a, s32* arc_b) {
    map_tile_t* tile;
    s32 tile_height;
    s32 floor;
    s32 peak;
    s32 height;
    s32 limit;
    s32 root;
    s32 gravity;
    s32 distance;
    s32 result;

    result = 0;
    tile = map_get_tile_data_ptr_from_battle_id(query->target.unit_id);
    tile_height = tile->height + tile->depth_half_height.depth;
    floor = (tile_height * 3) << 14;
    peak = effect_calculate_arc_height_term(query->delta.distance);
    if (query->delta.distance != 0) {
        height = query->target.height / 2;
        limit = (height + query->delta.y) * ONE;
        if (peak >= limit && floor >= limit) {
            query->target.height = height;
        } else {
            height = query->target.height * 3 / 4;
            limit = (height + query->delta.y) * ONE;
            if (peak >= limit && floor >= limit) {
                query->target.height = height;
            } else {
                limit = query->delta.y * ONE;
                if (peak < limit || floor < limit) {
                    return result;
                }
                query->target.height = 0;
            }
        }
        gravity = g_effect_gravity_modifier >> 6;
        distance = query->delta.distance >> 6;
        root = SquareRoot12((gravity - (limit >> 5)) * gravity - distance * distance);
        *arc_a = ((root + g_effect_gravity_modifier) << 8) / (query->delta.distance >> 4);
        *arc_b = ((g_effect_gravity_modifier - root) << 8) / (query->delta.distance >> 4);
        result = 1;
    }
    return result;
}

/*
 * Evaluate the arcing-weapon trajectory term a * b - (1 + a^2) * b^2 /
 * (2 * g_effect_gravity_modifier) in 20.12 fixed point.
 *
 * This is the height of a ballistic arc after a horizontal distance b
 * (`progress`), where a (`height`) is the tangent of the launch pitch in Q12:
 * the callers take the pitch itself as ratan2(height, ONE).
 *
 * Both operands are pre-shifted by range so the squares stay within 32 bits.
 * g_effect_gravity_modifier is the trajectory gravity modifier
 * (0xa8000).
 */
s32 effect_calculate_arc_trajectory_term(s32 height, s32 progress) {
    s32 divisor;
    s32 height_sq;
    s32 progress_sq;
    s32 linear;

    divisor = (g_effect_gravity_modifier * 2) >> 12;
    if (height >= -0x7fff && height <= 0x7fff) {
        height_sq = (height * height) >> 12;
        if (progress >= -0xfffff && progress <= 0xfffff) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (height * (progress >> 5)) >> 7;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (height * (progress >> 7)) >> 5;
        }
    } else {
        height >>= 4;
        height_sq = (height * height) >> 4;
        if (progress >= -0xfffff && progress <= 0xfffff) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (height * (progress >> 5)) >> 3;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (height * (progress >> 7)) >> 1;
        }
    }
    return linear - (((height_sq + ONE) >> 6) * (progress_sq >> 6)) / divisor;
}

/* Offset of a point current along a straight path of length total: dst is src,
 * the whole path's delta, scaled by current / total (a Q12 fraction). */
void effect_find_projectile_xyz_along_trajectory(const VECTOR* src, s32 total, s32 current, VECTOR* dst) {
    s32 progress;

    progress = (current << 8) / (total >> 4);
    dst->vx = (progress * src->vx) >> 12;
    dst->vy = (progress * src->vy) >> 12;
    dst->vz = (progress * src->vz) >> 12;
}

s32 effect_calculate_launch_velocity(void) {
    return SquareRoot12((g_effect_gravity_modifier * g_effect_gravity_scale) >> 12);
}

/*
 * List every present unit other than `first` and `second` that is neither
 * flagged by current-status bit 0x04 nor mount bit 0x40, with its tile
 * position and sprite height, into a scratchpad record returned via `out`.
 */
void effect_list_valid_unit_targeting_data(s32 first, s32 second, effect_obstacle_unit_list_t** out) {
    SVECTOR position;
    unit_stats_t* unit;
    s32 i;
    s32 slot;

    *out = (effect_obstacle_unit_list_t*)getScratchAddr(0);
    (*out)->count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_get_stats_from_battle_id(i);
        if (unit->entd_slot != first && unit->entd_slot != second && unit->entd_slot != BATTLE_ENTD_SLOT_NONE
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP))
            && !(unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            slot = (*out)->count++;
            (*out)->battle_ids[slot] = unit->entd_slot;
            battle_unit_get_tile_center_and_height_by_battle_id(unit->entd_slot, &position);
            (*out)->positions[slot] = position;
            (*out)->sprite_heights[slot] = battle_gfx_get_unit_spritesheet_height_by_unit_id(unit->entd_slot);
        }
    }
}

/*
 * Test a projectile position against the terrain and the listed units.
 *
 * Fills `tile_ref` with the map tile under `position` and returns 1 when the
 * position is inside that tile's solid column (either map layer) or inside a
 * listed unit's sprite box; the blocking unit's battle id is reported through
 * `out_unit`, which is otherwise left at -1.
 */
s32 effect_test_position_for_obstacle(
    effect_obstacle_unit_list_t* list, VECTOR* position, s32* out_unit, map_tile_coordinates_t* tile_ref) {
    map_tile_t* tile;
    s32 ceiling;
    s32 height;
    s32 y;
    s32 i;

    *out_unit = -1;
    tile_ref->x = position->vx / MAP_TILE_SIZE;
    tile_ref->y = position->vz / MAP_TILE_SIZE;
    tile = map_get_tile_data_pointer(tile_ref->x, tile_ref->y, 0);
    if (tile->ceiling_depth_and_marks.ceiling_depth == 0) {
        ceiling = tile->height;
    } else {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * 12) && y < -((height - ceiling) * 12)) {
        tile_ref->layer = 0;
        g_effect_trajectory_tile_flags = tile->flags._unknown_00;
        return 1;
    }

    tile = map_get_tile_data_pointer(tile_ref->x, tile_ref->y, 1);
    ceiling = 1;
    if (tile->ceiling_depth_and_marks.ceiling_depth != 0) {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * 12) && y < -((height - ceiling) * 12)) {
        tile_ref->layer = 1;
        g_effect_trajectory_tile_flags = tile->flags._unknown_00;
        return 1;
    }

    for (i = 0; i < list->count; i++) {
        if (position->vy < list->positions[i].vy && list->positions[i].vy - list->sprite_heights[i] < position->vy
            && list->sprite_heights[i] != 0 && list->positions[i].vx - 8 < position->vx
            && position->vx < list->positions[i].vx + 8 && list->positions[i].vz - 8 < position->vz
            && position->vz < list->positions[i].vz + 8) {
            *out_unit = list->battle_ids[i];
            return 1;
        }
    }
    return 0;
}

/*
 * Step a lobbed projectile along a ballistic arc. The launch pitch comes from
 * ratan2(height, ONE) and is published in both the arc descriptor and
 * g_effect_arctan_angle_mod; the per-step advance is the horizontal
 * component of the gravity impulse. Each step rotates the local (progress,
 * -trajectory term) offset into world space through the yaw matrix and tests
 * the result against the obstacle list, exactly as
 * effect_trace_projectile_path does for straight shots. On a clear
 * path g_effect_trajectory_step_count holds the full step count and the result is 1; on a hit it
 * holds the steps taken and the result is 0.
 *
 * `hit` is deliberately left uninitialized: the target reads $s6 after a
 * zero-step loop without ever writing it.
 */
s32 effect_trace_arc_trajectory_path(
    SVECTOR* origin, s32* height, s32* distance, SVECTOR* arc, effect_obstacle_unit_list_t* obstacles) {
    SVECTOR rotation;
    SVECTOR step;
    VECTOR position;
    VECTOR previous;
    MATRIX matrix;
    s32 angle;
    s32 velocity;
    s32 advance;
    s32 steps;
    s32 progress;
    s32 term;
    s32 hit;
    s32 i;

    angle = ratan2(*height, ONE);
    arc->vz = angle;
    g_effect_arctan_angle_mod = angle;
    progress = 0;
    /* The definition returns void; this site consumes the returned impulse. */
    velocity = effect_calculate_launch_velocity() >> 8;
    advance = (rcos(arc->vz) * velocity) >> 4;
    if (advance != 0) {
        steps = ((advance + *distance) - 1) / advance;
    } else {
        main_system_report_stub(1, 0);
        steps = 0;
    }

    rotation.vz = 0;
    rotation.vx = 0;
    rotation.vy = arc->vy;
    RotMatrix(&rotation, &matrix);
    position.vx = matrix.t[0] = origin->vx;
    i = 0;
    position.vy = matrix.t[1] = origin->vy;
    position.vz = matrix.t[2] = origin->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    step.vz = 0;
    for (; i < steps; i++) {
        previous = position;
        progress += advance;
        term = effect_calculate_arc_trajectory_term(*height, progress);
        step.vx = progress >> 12;
        step.vy = -term >> 12;
        RotTrans(&step, &position, &position.pad);
        hit = effect_test_position_for_obstacle(
            obstacles, &position, &g_effect_trajectory_hit_unit_id, &g_effect_trajectory_tile);
        if (hit != 0) {
            break;
        }
    }

    setVector(
        &g_effect_trajectory_step, position.vx - previous.vx, position.vy - previous.vy, position.vz - previous.vz);
    copyVector(&g_effect_trajectory_position, &position);
    if (hit != 0) {
        g_effect_trajectory_step_count = i;
        return 0;
    }
    g_effect_trajectory_step_count = steps;
    return 1;
}

/* Test whether either arcing projectile path reaches the target unit.
 *
 * The arc counterpart of effect_check_direct_trajectory_between_units: the
 * source is raised by two thirds of its sprite height, effect_calculate_arcing_trajectory yields
 * the two candidate arcs for the offset, and each is traced against the
 * other units' positions. Returns 1 when an arc is clear. */
s32 effect_check_arc_trajectory_between_units(u8 source_id, u8 target_id) {
    SVECTOR rotation;
    battle_arc_query_t query;
    effect_obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;

    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    query.target.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id);
    effect_list_valid_unit_targeting_data(source_id, target_id, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &query.source.position);
    query.source.position.vy -= query.source.height;
    battle_unit_get_tile_center_and_height_by_battle_id(target_id, &query.target.position);
    query.delta.x = query.target.position.vx - query.source.position.vx;
    query.delta.z = query.target.position.vz - query.source.position.vz;
    query.delta.y = query.source.position.vy - query.target.position.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    rotation.vy = ratan2(-query.delta.z, query.delta.x);
    if (effect_calculate_arcing_trajectory(&query, &arc_a, &arc_b) != 0
        && (effect_trace_arc_trajectory_path(
                &query.source.position, &arc_a, &query.delta.distance, &rotation, obstacles)
                != 0
            || effect_trace_arc_trajectory_path(
                   &query.source.position, &arc_b, &query.delta.distance, &rotation, obstacles)
                != 0)) {
        return 1;
    }
    return 0;
}

/*
 * Set up an arcing projectile from source_id toward target_id (or the tile at
 * tile_position when target_id is -1) and pick the arc to fly.
 *
 * Both candidate arcs from effect_calculate_arcing_trajectory are
 * traced, arc_b first; a clear arc returns target_id. Otherwise an arc blocked
 * by a unit of another team than the source wins (arc_b's first) and returns
 * that unit. Failing that, arc_a's trace is kept and its blocker (or -1) is
 * returned, except when only arc_a was blocked by a unit: then arc_b's
 * terrain-blocked result is restored and -1 returned. g_effect_arc_trajectory_height receives the
 * chosen arc height. `unused` reserves the target's 16 otherwise unused frame bytes.
 */
s32 effect_set_and_validate_arc_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    s32 unused[4];
    SVECTOR arc;
    VECTOR second_step;
    VECTOR first_step;
    SVECTOR second_position;
    SVECTOR first_position;
    battle_arc_query_t query;
    effect_obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;
    map_tile_t* tile;
    unit_stats_t* stats;
    unit_stats_t* other;
    s32 hit;
    s32 second_hit;
    s32 first_unit;
    s32 first_other;
    s32 first_count;
    s32 second_unit;
    s32 second_other;
    s32 second_count;
    s32 result;
    s32 height;

    g_effect_trajectory_source_id = source_id;
    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        query.target.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id);
    } else {
        query.target.height = 0;
    }
    effect_list_valid_unit_targeting_data(source_id, -1, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_effect_trajectory_origin);
    g_effect_trajectory_origin.vy -= query.source.height;
    if (target_id == -1) {
        tile = map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vy = -((tile->height + tile->depth_half_height.depth) * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_effect_trajectory_destination);
    }
    query.delta.x = g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx;
    query.delta.z = g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz;
    query.delta.y = g_effect_trajectory_origin.vy - g_effect_trajectory_destination.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    arc.vy = ratan2(-query.delta.z, query.delta.x);
    if (effect_calculate_arcing_trajectory(&query, &arc_a, &arc_b) == 0) {
        arc_b = ONE;
        arc_a = ONE;
    }

    hit = effect_trace_arc_trajectory_path(&g_effect_trajectory_origin, &arc_b, &query.delta.distance, &arc, obstacles);
    first_unit = g_effect_trajectory_hit_unit_id;
    first_step = g_effect_trajectory_step;
    first_position = g_effect_trajectory_position;
    first_count = g_effect_trajectory_step_count;
    first_other = -1;
    if (g_effect_trajectory_hit_unit_id == -1) {
        first_other = g_effect_trajectory_tile_flags;
    }
    if (hit != 0) {
        height = arc_b;
        result = target_id;
    } else {
        second_hit = effect_trace_arc_trajectory_path(
            &g_effect_trajectory_origin, &arc_a, &query.delta.distance, &arc, obstacles);
        second_unit = g_effect_trajectory_hit_unit_id;
        second_step = g_effect_trajectory_step;
        second_position = g_effect_trajectory_position;
        second_count = g_effect_trajectory_step_count;
        second_other = -1;
        if (g_effect_trajectory_hit_unit_id == -1) {
            second_other = g_effect_trajectory_tile_flags;
        }
        if (second_hit != 0) {
            height = arc_a;
            result = target_id;
        } else {
            stats = battle_unit_get_stats_from_battle_id(source_id);
            if (first_unit != -1
                && (other = battle_unit_get_stats_from_battle_id(first_unit),
                    (stats->team_flags & BATTLE_TEAM_MASK) != (other->initial_team_flags & BATTLE_TEAM_MASK))) {
                g_effect_trajectory_step = first_step;
                g_effect_trajectory_position = first_position;
                height = arc_b;
                g_effect_trajectory_hit_unit_id = first_unit;
                result = first_unit;
                g_effect_trajectory_step_count = first_count;
            } else if (second_unit != -1
                && (other = battle_unit_get_stats_from_battle_id(second_unit),
                    (stats->team_flags & BATTLE_TEAM_MASK) != (other->initial_team_flags & BATTLE_TEAM_MASK))) {
                height = arc_a;
                g_effect_trajectory_step_count = second_count;
                g_effect_trajectory_hit_unit_id = second_unit;
                result = second_unit;
            } else if (second_unit == -1) {
                height = arc_a;
                g_effect_trajectory_step_count = second_count;
                g_effect_trajectory_hit_unit_id = second_unit;
                g_effect_trajectory_tile_flags = second_other;
                result = -1;
            } else if (first_unit != -1) {
                height = arc_a;
                g_effect_trajectory_step_count = second_count;
                g_effect_trajectory_tile_flags = second_other;
                g_effect_trajectory_hit_unit_id = second_unit;
                result = second_unit;
            } else {
                g_effect_trajectory_step = first_step;
                g_effect_trajectory_position = first_position;
                height = arc_b;
                g_effect_trajectory_hit_unit_id = first_unit;
                result = -1;
                g_effect_trajectory_tile_flags = first_other;
                g_effect_trajectory_step_count = first_count;
            }
        }
    }
    g_effect_arc_trajectory_height = height;
    return result;
}

/* Wrap the current bow-shot yaw into (-0x800, 0x800] then bucket it into
   one of three arc quadrants: 0 = behind, 1 = side, 2 = ahead. */
s32 effect_get_bow_shot_angle(void) {
    s16 angle = g_effect_arctan_angle_mod;
    if (angle >= 0x801) {
        g_effect_arctan_angle_mod = angle - ONE;
    }
    {
        s16 angle_after_hi = g_effect_arctan_angle_mod;
        if (angle_after_hi < -0x800) {
            g_effect_arctan_angle_mod = angle_after_hi + ONE;
        }
    }
    {
        s16 wrapped = g_effect_arctan_angle_mod;
        if (wrapped < 0x2AB) {
            return wrapped >= -0x2AA;
        }
        return 2;
    }
}

/*
 * Step a projectile from `origin` along `delta` in 0x2000 increments of the
 * `distance` total, testing each position against the obstacle list. Stores
 * the last step's movement and the final position, sets g_effect_trajectory_step_count to a
 * quarter of the steps taken, and returns 1 when the path is clear or 0 when
 * an obstacle stopped it. The `step` variable keeps the target's register
 * divide; a literal 0x2000 folds to a shift.
 */
s32 effect_trace_projectile_path(
    VECTOR* delta, SVECTOR* origin, s32* distance, effect_obstacle_unit_list_t* obstacles) {
    VECTOR position;
    VECTOR previous;
    s32 step;
    s32 steps;
    s32 progress;
    s32 hit;
    s32 i;

    step = 0x2000;
    steps = *distance / step;
    progress = 0;
    position.vx = origin->vx;
    hit = 0;
    position.vy = origin->vy;
    i = 0;
    position.vz = origin->vz;
    for (; i < steps; i++) {
        previous = position;
        progress += step;
        effect_find_projectile_xyz_along_trajectory(delta, *distance, progress, &position);
        position.vx += origin->vx;
        position.vy += origin->vy;
        position.vz += origin->vz;
        hit = effect_test_position_for_obstacle(
            obstacles, &position, &g_effect_trajectory_hit_unit_id, &g_effect_trajectory_tile);
        if (hit != 0) {
            break;
        }
    }
    setVector(
        &g_effect_trajectory_step, position.vx - previous.vx, position.vy - previous.vy, position.vz - previous.vz);
    copyVector(&g_effect_trajectory_position, &position);
    if (hit != 0) {
        g_effect_trajectory_step_count = i / 4;
        return 0;
    }
    g_effect_trajectory_step_count = steps / 4;
    return 1;
}

/*
 * Trace a projectile path from one unit's upper body to another's.
 *
 * Both endpoints are raised by two thirds of the unit's sprite height (12 when
 * the target has none). `unused_20` and `unused_38` reproduce unreferenced
 * frame slots the target reserves between the vectors.
 */
s32 effect_check_direct_trajectory_between_units(u8 source_id, u8 target_id) {
    SVECTOR origin;
    SVECTOR destination;
    SVECTOR unused_20;
    VECTOR delta;
    s32 unused_38[2];
    effect_obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 2 / 3;
    if (target_height == 0) {
        target_height = 12;
    }
    effect_list_valid_unit_targeting_data(source_id, target_id, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &origin);
    origin.vy -= source_height;
    battle_unit_get_tile_center_and_height_by_battle_id(target_id, &destination);
    destination.vy -= target_height;
    setVector(&delta, destination.vx - origin.vx, destination.vy - origin.vy, destination.vz - origin.vz);
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    return effect_trace_projectile_path(&delta, &origin, &distance, obstacles);
}

/*
 * Sets up an arcing projectile trajectory between a source unit and either a
 * target unit or a bare tile, and reports whether the flight is obstructed.
 *
 * Both endpoints are raised by two thirds of their sprite height (12 when the
 * target has none), as in effect_check_direct_trajectory_between_units;
 * the tile case instead centers on the tile at tile_position (x, layer, y) and
 * sits on its surface, as in effect_set_arrow_trajectory.  The extra
 * work over those two is the launch angle: ratan2 of the vertical delta
 * against the horizontal distance, biased by 0x400 (a quarter turn) into
 * g_effect_arctan_angle_mod.  Returns target_id when the path is clear
 * and the blocking unit from g_effect_trajectory_hit_unit_id when it is not.
 */
s32 effect_check_direct_trajectory_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    map_tile_t* tile;
    effect_obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    g_effect_trajectory_source_id = source_id;
    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 2 / 3;
        if (target_height == 0) {
            target_height = 12;
        }
    }
    effect_list_valid_unit_targeting_data(source_id, -1, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_effect_trajectory_origin);
    g_effect_trajectory_origin.vy -= source_height;
    if (target_id == -1) {
        tile = map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vy = -(tile->height * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_effect_trajectory_destination);
        g_effect_trajectory_destination.vy -= target_height;
    }
    setVector(&delta, g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx,
        g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy,
        g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz);
    g_effect_arctan_angle_mod = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    if (effect_trace_projectile_path(&delta, &g_effect_trajectory_origin, &distance, obstacles) != 0) {
        return target_id;
    }
    return g_effect_trajectory_hit_unit_id;
}

/* Sets an arrow's source, destination and flight duration.
 *
 * The projectile origin is the first unit raised by two thirds of its
 * sprite height, and the destination is the second unit raised by three
 * quarters of its height, or, when target_id is -1, to the center of the tile
 * at tile_position (x, layer, y) on its surface. g_effect_trajectory_step_count receives the
 * flight duration derived from the distance, as in
 * effect_check_direct_trajectory_between_units / effect_trace_projectile_path.
 * `delta` reproduces the target's stores to an otherwise unused stack vector.
 */
void effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    map_tile_t* tile;
    s32 source_height;
    s32 target_height;

    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 3 / 4;
    }
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_effect_trajectory_origin);
    g_effect_trajectory_origin.vy -= source_height;
    if (target_id == -1) {
        tile = map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vy = -(tile->height * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_effect_trajectory_destination);
        g_effect_trajectory_destination.vy -= target_height;
    }
    setVector(&delta, g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx,
        g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy,
        g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz);
    g_effect_trajectory_step_count
        = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE) / 8 >> 12;
}

/* Keep the trap animation table as u8[]: another element type introduces an
 * 8-byte compiler frame and changes the load-delay and return slots. */

/* Start the animation script of trap group on slot: reset the color and
 * script state and store the 16-bit CLUT value (callers pass CLUTs of
 * MAP_CLUT_ROW_WEAPONS) at 0x88. */
void effect_init_trap_animation(s32 group, u16 clut, effect_slot_t* slot) {
    u32 offset;

    slot->clut = clut;
    offset = *(u16*)(g_effect_trap_animation_data + (g_effect_trap_particle_configs[group].trap_animation << 1));
    slot->color.b = 0x80;
    slot->color.g = 0x80;
    slot->color.r = 0x80;
    slot->step_index = 0;
    slot->hold = 0;
    slot->angle = 0;
    slot->script = (effect_anim_script_t*)(g_effect_trap_animation_data + offset);
}

/* Spawn this frame's share of a particle group's effect slots, drawn with CLUT
 * clut_index of the weapons row.
 *
 * The group spawns while the secondary effect's timer is inside its spawn
 * frames. Its spawns_per_frame caps how many slots one frame may start; the
 * secondary effect's active_count counts the live slots and stops the
 * spawning at the group's total count. */
void effect_start_group(s32 group, s32 clut_index) {
    effect_slot_t* slot;
    s32 spawned;
    s32 i;
    u8 id;

    if (g_effect_trap_particle_configs[group].count > g_effect_current_secondary->active_count
        && g_effect_current_secondary->timer >= g_effect_trap_particle_configs[group].spawn_start_frame
        && g_effect_current_secondary->timer < g_effect_trap_particle_configs[group].spawn_end_frame) {
        for (spawned = 0; spawned < g_effect_trap_particle_configs[group].spawns_per_frame; spawned++) {
            for (i = 0; i < g_effect_trap_particle_configs[group].count; i++) {
                if (g_effect_current_secondary->slot_ids[i] == 0) {
                    break;
                }
            }
            id = effect_alloc_slot();
            g_effect_current_secondary->slot_ids[i] = id;
            if (id == 0) {
                break;
            }
            slot = &g_effect_slots[g_effect_current_secondary->slot_ids[i]];
            slot->life = effect_spawn_particle_motion(&g_effect_trap_particle_configs[group], &slot->motion);
            effect_init_trap_animation(group, clut_index + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
            g_effect_current_secondary->active_count++;
            if (g_effect_current_secondary->active_count == g_effect_trap_particle_configs[group].count) {
                break;
            }
        }
    }
}

/* Advance the current secondary effect's particles: every effect slot in the
 * first count entries of slot_ids, count being the particle group's. A slot that
 * has finished is freed and active_count drops by one. */
void effect_update_active_slots(s32 group) {
    s32 i;
    s32 id;

    for (i = 0; i < g_effect_trap_particle_configs[group].count; i++) {
        id = g_effect_current_secondary->slot_ids[i];
        if (id != 0 && effect_update_slot(id) == 0) {
            effect_free_slot(id);
            g_effect_current_secondary->slot_ids[i] = 0;
            g_effect_current_secondary->active_count--;
        }
    }
}

/* Secondary effect function 0x00 in g_effect_secondary_handlers: its
 * result of 0 makes effect_update_secondary_effects end the effect at
 * once. */
s32 battle_return_zero_801b0fec(void) {
    return 0;
}

void battle_noop_801b0ff4(void) {
}

/*
 * Secondary-effect handler for bow and crossbow arrows.
 *
 * Phase 1 derives the arrow's heading, launch pitch and per-step gravity from
 * the trajectory origin/destination, builds its rotation matrix and allocates
 * the polygon work buffer. Phase 2 advances one step along the arc, orients
 * the arrow along the local slope and draws it with effect_draw_projectile_model; the last
 * step or a positive height ends the flight. Phase 3 returns 0. An unknown
 * phase returns `result` uninitialized, as the target returns $s6 unwritten.
 */
s32 effect_arrow_arc_secondary_handler(void) {
    SVECTOR rotation;
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    effect_arrow_buffer_t* work;
    s32 velocity;
    s32 term;
    s32 previous;
    s32 next;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;
    u8 flags;

    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        delta.vx = g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx;
        delta.vz = g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz;
        delta.vy = g_effect_trajectory_origin.vy - g_effect_trajectory_destination.vy;
        g_effect_arrow_arc_length = SquareRoot12((delta.vx * delta.vx + delta.vz * delta.vz) * ONE);
        g_effect_arrow_arc_yaw = ratan2(-delta.vz, delta.vx);
        result = 1;
        g_effect_arrow_arc_caster_relative_yaw = g_effect_arrow_arc_yaw
            - (((battle_unit_get_facing_quadrant_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id)
                    + 1)
                   & 3)
                << 10);
        g_effect_arrow_arc_height = g_effect_arc_trajectory_height;
        g_effect_arrow_arc_launch_angle = ratan2(g_effect_arc_trajectory_height, ONE);
        g_effect_arrow_arc_distance = 0;
        velocity = effect_calculate_launch_velocity() >> 8;
        g_effect_arrow_arc_distance_step = (rcos(g_effect_arrow_arc_launch_angle) * velocity) >> 4;
        g_effect_projectile_step_count = g_effect_trajectory_step_count;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = g_effect_arrow_arc_yaw;
        RotMatrix(&rotation, &g_effect_projectile_matrix);
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        if (g_effect_current_secondary->allocation != 0) {
            main_heap_free(g_effect_current_secondary->allocation);
        }
        work = main_heap_alloc(0xc88);
        g_effect_current_secondary->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 4; j++) {
                SetPolyG3(&work->g3[i][j]);
            }
            for (j = 0; j < 13; j++) {
                SetPolyG4(&work->g4[i][j]);
            }
        }
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        previous = -effect_calculate_arc_trajectory_term(g_effect_arrow_arc_height, g_effect_arrow_arc_distance);
        g_effect_arrow_arc_distance += g_effect_arrow_arc_distance_step;
        term = -effect_calculate_arc_trajectory_term(g_effect_arrow_arc_height, g_effect_arrow_arc_distance);
        next = -effect_calculate_arc_trajectory_term(
            g_effect_arrow_arc_height, g_effect_arrow_arc_distance + g_effect_arrow_arc_distance_step);
        setVector(
            &angles, 0, g_effect_arrow_arc_yaw, ratan2(next - previous, g_effect_arrow_arc_distance_step * 2) + 0x400);
        SetRotMatrix(&g_effect_projectile_matrix);
        SetTransMatrix(&g_effect_projectile_matrix);
        rotation.vy = term >> 12;
        rotation.vz = 0;
        rotation.vx = g_effect_arrow_arc_distance >> 12;
        RotTrans(&rotation, &delta, &delta.pad);
        setVector(&position, g_effect_trajectory_origin.vx + delta.vx, g_effect_trajectory_origin.vy + delta.vy,
            g_effect_trajectory_origin.vz + delta.vz);
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        setVector(&rotation, g_effect_trajectory_origin.vx + delta.vx, g_effect_trajectory_origin.vy + delta.vy,
            g_effect_trajectory_origin.vz + delta.vz);
        RotTrans(&rotation, &delta, &delta.pad);
        scale.vz = 0x40;
        scale.vy = 0x40;
        scale.vx = 0x40;
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        spin.vz = 0;
        spin.vy = 0;
        spin.vx = 0;
        effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        if (g_effect_projectile_step_count == 2 && g_effect_trajectory_hit_unit_id != -1
            && g_effect_current_secondary->target_count != 0) {
            battle_unit_call_bow_hardcoding_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id,
                g_effect_current_secondary->target.record.data.unit.runtime_id);
        }
        if (--g_effect_projectile_step_count == 0 || position.vy >= 0) {
            g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
            flags = g_effect_current_secondary->target.record.data.unit.result_animation;
            if (flags == EFFECT_HIT_MISSED || flags - 4 < 2U || g_effect_trajectory_hit_unit_id == -1) {
                D_801B8B98 = 0x80000000;
                effect_end_secondary_on_current_unit();
            }
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/* 0x28-byte heap state kept in the secondary effect's allocation. */
typedef struct effect_burst_state {
    s32 frame;    /* 0x00 */
    s32 done;     /* 0x04 */
    s32 count_a;  /* 0x08 */
    u8 ids_a[8];  /* 0x0c; group 0 slots */
    s32 count_b;  /* 0x14 */
    u8 ids_b[16]; /* 0x18; group 1 or 9 slots */
} effect_burst_state_t;

/* Secondary effect function 0x02 in g_effect_secondary_handlers: the burst of a weapon
 * hit (EFFECT_SECONDARY_ANIMATION_WEAPON_HIT). */
s32 effect_weapon_hit_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_burst_state_t* state;
    effect_slot_t* slot;
    s32 unused[8];
    vec3s32_t gravity;
    VECTOR zoom;
    VECTOR offset;
    s32 result;
    s32 group;
    s32 j;
    s32 done;
    s32 i;
    u8 id;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        if (effect->target.record.data.unit.result_animation == EFFECT_HIT_NO_EFFECT) {
            return 0;
        }
        if (effect->allocation != 0) {
            main_heap_free(effect->allocation);
        }
        state = main_heap_alloc(sizeof(effect_burst_state_t));
        g_effect_current_secondary->allocation = state;
        state->frame = 0;
        state->done = 0;
        state->count_a = 0;
        state->count_b = 0;
        for (i = 0; i < 8; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 16; i++) {
            state->ids_b[i] = 0;
        }
        result = 1;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        effect_copy_camera_angles_and_world_coords();
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        state = effect->allocation;
        group = EFFECT_PARTICLE_GROUP_WEAPON_HIT_B;
        if (effect->target.record.data.unit.result_animation == EFFECT_HIT_MISSED) {
            group = EFFECT_PARTICLE_GROUP_WEAPON_HIT_B_MISSED;
        }
        if (group == EFFECT_PARTICLE_GROUP_WEAPON_HIT_B) {
            if (state->frame == 0) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    MAP_COLOR_MODE_BASE_HALF, 0, effect->target.record.data.unit.runtime_id, 0x1f, 0x1f, 0x1f);
            }
            if (state->frame == group) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    MAP_COLOR_MODE_RESTORE, 2, g_effect_current_secondary->target.record.data.unit.runtime_id, 0, 0, 0);
            }
        }
        if (state->frame < 4) {
            if (state->count_a < 8) {
                for (i = 0; i < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A].spawns_per_frame;
                    i++) {
                    for (j = 0; j < 8; j++) {
                        if (state->ids_a[j] == 0) {
                            break;
                        }
                    }
                    state->ids_a[j] = effect_alloc_slot();
                    if (state->ids_a[j] == 0) {
                        break;
                    }
                    slot = &g_effect_slots[state->ids_a[j]];
                    slot->life = effect_spawn_particle_motion(
                        &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A], &slot->motion);
                    effect_init_trap_animation(EFFECT_PARTICLE_GROUP_WEAPON_HIT_A,
                        g_effect_current_secondary->parameter + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
                    state->count_a++;
                    if (state->count_a == 8) {
                        break;
                    }
                }
            }
            if (state->frame < 4 && state->count_b < 16) {
                for (i = 0; i < g_effect_trap_particle_configs[group].spawns_per_frame; i++) {
                    for (j = 0; j < 16; j++) {
                        if (state->ids_b[j] == 0) {
                            break;
                        }
                    }
                    state->ids_b[j] = effect_alloc_slot();
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                    slot = &g_effect_slots[state->ids_b[j]];
                    slot->life = effect_spawn_particle_motion(&g_effect_trap_particle_configs[group], &slot->motion);
                    effect_init_trap_animation(
                        EFFECT_PARTICLE_GROUP_WEAPON_HIT_B, MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 10), slot);
                    slot->color.r = slot->color.g = slot->color.b = 0xff;
                    state->count_b++;
                    if (state->count_b == 16) {
                        break;
                    }
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        for (i = 0; i < 8; i++) {
            id = state->ids_a[i];
            if (id != 0 && effect_update_slot(id) == 0) {
                effect_free_slot(id);
                state->ids_a[i] = 0;
                state->count_a--;
            }
        }
        for (i = 0; i < 16; i++) {
            id = state->ids_b[i];
            if (id != 0 && effect_update_slot(id) == 0) {
                effect_free_slot(id);
                state->ids_b[i] = 0;
                state->count_b--;
            }
        }
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        if (g_effect_current_secondary->target.record.data.unit.result_animation == EFFECT_HIT_CRITICAL) {
            if (state->frame == 0) {
                zoom.vx = zoom.vy = zoom.vz = -0x80;
                camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 2) {
                zoom.vx = zoom.vy = zoom.vz = 0x2e4;
                camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 4) {
                zoom.vx = zoom.vy = zoom.vz = 0;
                camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 12, &zoom);
                offset.vx = offset.vy = offset.vz = 4;
                camera_execute_position_command(
                    EFFECT_CAMERA_SOURCE_GIVEN | EFFECT_CAMERA_MODE_SHAKE_DECAYING, 0, 9, &offset);
            }
        }
        done = state->done;
        result = done != 1;
        state->frame++;
        if (state->count_a == 0 && state->count_b == 0 && state->frame >= 31) {
            state->done = done + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x03 in g_effect_secondary_handlers: the
 * particles of EFFECT_SECONDARY_ANIMATION_GOLEM.
 *
 * Initializing clears the counters. Executing starts this frame's share of the
 * Golem particle group with CLUT 13 of the weapons row and, like Finalizing,
 * steps the live slots under the secondary-effect inertia threshold and a gravity
 * of (0, ONE, 0), then puts back the effect file's own values. Once no slot is
 * live and the timer has passed the group's spawn_start_frame, finish_timer
 * counts up; the handler returns 0 on the frame after that begins, and 1 until
 * then. */
s32 effect_golem_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6];
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_start_group(EFFECT_PARTICLE_GROUP_GOLEM, 13);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(EFFECT_PARTICLE_GROUP_GOLEM);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_GOLEM].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* The target compares a line's two endpoints as 32-bit values. */
#define LINE_G2_XY_WORD(line, n) (*(s32*)&line->x##n)

/* Secondary handler for a charge effect: phase 1 loads the animation's
 * parameters and primes the line primitives, phase 2 spawns trails that
 * converge on the caster, and phases 2-3 advance and draw them.
 *
 * The OT link masks `line` before `setaddr(line, getaddr(ot + 1))`: that keeps
 * the 0xffffff mask's loop lifetime short enough for loop.c to leave it in the
 * inner loop, and getaddr's extra mask reference gives it priority over the
 * 0xff000000 mask in local allocation (a1 versus a2 in the target). */
s32 effect_spell_charge_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_charge_state_t* state;
    SVECTOR in;
    s32 unused[4];
    VECTOR out;
    LINE_G2* line;
    SVECTOR* screen;
    effect_slot_t* slot;
    u32* ot;
    s16* offsets;
    s16* offsets_y;
    s32 result;
    s32 index;
    s32 i;
    s32 j;
    s32 k;
    s32 age;
    s32 head;
    s32 fade;
    s32 x;
    s32 y;
    s32 dir;
    s32 over;
    u32 red;
    u32 green;
    u32 blue;
    u32 link;
    effect_slot_t* work;

    effect = g_effect_current_secondary;
    state = &g_effect_secondary_state.charge;
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        index = g_effect_secondary_animations[effect->animation_id].parameter;
        g_effect_current_secondary->parameter = effect_get_element_index(effect->parameter);
        g_effect_secondary_state.charge.speed = g_effect_charge_line_params[index].speed;
        g_effect_secondary_state.charge.spread_range = g_effect_charge_line_params[index].spread_range;
        g_effect_secondary_state.charge.spread_offset = g_effect_charge_line_params[index].spread_offset;
        g_effect_secondary_state.charge.duration = g_effect_charge_line_params[index].duration;
        g_effect_secondary_state.charge.radius = g_effect_charge_line_params[index].radius;
        g_effect_secondary_state.charge.spawn_interval = g_effect_charge_line_params[index].spawn_interval;
        g_effect_secondary_state.charge.color = g_effect_current_secondary->parameter;
        g_effect_secondary_state.charge.max_trails = g_effect_charge_line_params[index].max_trails;
        g_effect_secondary_state.charge.fade_row = g_effect_charge_line_params[index].fade_row;
        SetDrawMode(&state->mode[0], 1, 1, 0x20, 0);
        SetDrawMode(&state->mode[1], 1, 1, 0x20, 0);
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 96; j++) {
                SetLineG2(&state->lines[i][j]);
                SetSemiTrans(&state->lines[i][j], 1);
            }
        }
        for (i = 0; i < 16; i++) {
            state->trails[i].slot_id = 0;
        }
        state->count.trails = 0;
        state->count.auxiliary_particles = 0;
        state->count.particles = 0;
        for (i = 0; i < 2; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 14; i++) {
            state->ids_b[i] = 0;
        }
        screen = battle_unit_get_world_coords_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id);
        i = battle_gfx_get_unit_spritesheet_height_by_runtime_id(
                g_effect_current_secondary->caster.data.unit.runtime_id)
            + 8;
        g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].position.y
            = g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE_13].position.y = -i;
        in.vx = screen->vx;
        in.vz = screen->vz;
        in.vy = screen->vy - i;
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&in, &out, &out.pad);
        state->angle = 0;
        state->done = 0;
        state->x = out.vx;
        state->y = out.vy;
        suzuki_sound_play_sfx(MAIN_SFX_SPELL_CHARGE);
        result = 1;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        if (g_effect_secondary_state.charge.done == 0
            && g_effect_secondary_state.charge.count.trails < g_effect_secondary_state.charge.max_trails
            && rand() % g_effect_secondary_state.charge.spawn_interval == 0) {
            for (i = 0; i < 16; i++) {
                if (state->trails[i].slot_id == 0) {
                    j = state->trails[i].slot_id = effect_alloc_slot();
                    if (j == 0) {
                        break;
                    }
                    state->count.trails++;
                    k = (rand() & 0x1ff) + state->angle;
                    state->angle += 0x571;
                    g_effect_slots[j].motion.position.x = rcos(k) * state->radius + (state->x * ONE);
                    g_effect_slots[j].motion.position.y = rsin(k) * state->radius + (state->y * ONE);
                    k = k + rand() % state->spread_range - state->spread_offset - 0x800;
                    g_effect_slots[j].motion.velocity.x = rcos(k) * state->speed;
                    g_effect_slots[j].motion.velocity.y = rsin(k) * state->speed;
                    g_effect_slots[j].life = 1;
                    for (k = 0; k < 7; k++) {
                        state->trails[i].points[k].vx = g_effect_slots[j].motion.position.x >> 12;
                        state->trails[i].points[k].vy = g_effect_slots[j].motion.position.y >> 12;
                    }
                    state->trails[i].age = 0;
                    break;
                }
            }
        }
        if (state->count.particles < 14) {
            for (i = 0; i < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].spawns_per_frame; i++) {
                for (j = 0; j < 14; j++) {
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                }
                state->ids_b[j] = effect_alloc_slot();
                if (state->ids_b[j] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids_b[j]];
                slot->life = effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE], &slot->motion);
                effect_init_trap_animation(
                    EFFECT_PARTICLE_GROUP_SPELL_CHARGE, MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 15), slot);
                state->count.particles++;
                if (state->count.particles == 14) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        for (i = 0; i < 2; i++) {
            j = state->ids_a[i];
            if (j != 0 && effect_update_slot(j) == 0) {
                effect_free_slot(j);
                state->ids_a[i] = 0;
                state->count.auxiliary_particles--;
            }
        }
        for (i = 0; i < 14; i++) {
            j = state->ids_b[i];
            if (j != 0 && effect_update_slot(j) == 0) {
                effect_free_slot(j);
                state->ids_b[i] = 0;
                state->count.particles--;
            }
        }
        red = g_effect_trap_colors[state->color].r;
        green = g_effect_trap_colors[state->color].g;
        blue = g_effect_trap_colors[state->color].b;
        offsets = &g_effect_charge_line_direction_offsets[0].vx;
        offsets_y = &g_effect_charge_line_direction_offsets[0].vy;
        for (i = 0; i < 16; i++) {
            j = state->trails[i].slot_id;
            if (j != 0) {
                work = &g_effect_slots[j];
                work->motion.position.x = x = g_effect_slots[j].motion.position.x + g_effect_slots[j].motion.velocity.x;
                work->motion.position.y = y = g_effect_slots[j].motion.position.y + g_effect_slots[j].motion.velocity.y;
                age = ++state->trails[i].age;
                head = age % 7;
                if (age <= state->duration) {
                    state->trails[i].points[head].vx
                        = effect_interpolate_s32_cos(x >> 12, state->x, state->duration, age);
                    fade = 6;
                    state->trails[i].points[head].vy
                        = effect_interpolate_s32_cos(y >> 12, state->y, state->duration, age);
                } else {
                    state->trails[i].points[head].vx = state->x;
                    state->trails[i].points[head].vy = state->y;
                    over = age - 6;
                    fade = state->duration - over;
                    if (fade < 0) {
                        fade = 0;
                    }
                }
                for (k = 0; k < 6; k++) {
                    line = &state->lines[g_effect_buffer_index][i * 6 + k];
                    line->x0 = state->trails[i].points[head].vx;
                    line->y0 = state->trails[i].points[head].vy;
                    setRGB0(line, red * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        green * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        blue * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8);
                    if (--head < 0) {
                        head = 6;
                    }
                    age--;
                    if (fade != 0) {
                        fade--;
                    }
                    if (age < 0) {
                        fade = 0;
                    }
                    line->x1 = state->trails[i].points[head].vx;
                    line->y1 = state->trails[i].points[head].vy;
                    setRGB1(line, red * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        green * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        blue * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8);
                    if (LINE_G2_XY_WORD(line, 0) != LINE_G2_XY_WORD(line, 1)) {
                        dir = (ratan2(line->y1 - line->y0, line->x1 - line->x0) + 0x900) / 512;
                        line->x1 += offsets[dir * 2];
                        line->y1 += offsets_y[dir * 2];
                        ot = g_effect_polygon_depth_data;
                        link = (u32)line & 0xffffff;
                        setaddr(line, getaddr(ot + 1));
                        ot[1] = (ot[1] & 0xff000000) | link;
                    }
                }
                if (state->trails[i].age >= state->duration + 7) {
                    effect_free_slot(j);
                    state->trails[i].slot_id = 0;
                    state->count.trails--;
                }
            }
        }
        AddPrim(g_effect_polygon_depth_data + 1, &state->mode[g_effect_buffer_index]);
        if (g_effect_current_secondary->phase == EFFECT_SECONDARY_PHASE_EXECUTING
            || (state->count.trails != 0 || state->count.auxiliary_particles != 0 || state->count.particles != 0)) {
            result = 1;
        } else {
            suzuki_sound_stop_sfx_channels(MAIN_SFX_SPELL_CHARGE);
            result = 0;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x05: an erroneous
 * secondary animation that only steps its state so that a null animation can
 * finish cleanly.  Returns 1 to keep animating, 0 to stop.
 */
s32 effect_null_secondary_handler(void) {
    s32 result;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x06 (Red Orbs) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 8, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 effect_red_orbs_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(MAIN_SFX_RED_ORBS);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_start_group(EFFECT_PARTICLE_GROUP_RED_ORBS, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(EFFECT_PARTICLE_GROUP_RED_ORBS);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_RED_ORBS].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x07 in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2968(void) {
}

/*
 * Secondary effect function 0x08 (the dust of a fall) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 effect_fall_dust_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_current_secondary->active_count = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_start_group(EFFECT_PARTICLE_GROUP_FALL_DUST, 9);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(EFFECT_PARTICLE_GROUP_FALL_DUST);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_FALL_DUST].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x09 (Splash) in g_effect_secondary_handlers.
 * It shares the fall dust
 * handler's shape; each executing frame first lifts group 7 to the water
 * surface of the target's tile, 12 units per height and depth step above the
 * target's screen z. Returns 1 to keep animating.
 */
s32 effect_splash_secondary_handler(void) {
    effect_secondary_data_t* effect;
    map_tile_t* tile;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_current_secondary->active_count = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        tile = map_get_unit_tile_from_world_coords(g_effect_current_secondary->target.record.data.unit.runtime_id);
        g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].position.y
            = -((tile->height + tile->depth_half_height.depth) * 12)
            - battle_unit_get_world_coords_by_runtime_id(g_effect_current_secondary->target.record.data.unit.runtime_id)
                  ->vy;
        effect_start_group(EFFECT_PARTICLE_GROUP_SPLASH, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(EFFECT_PARTICLE_GROUP_SPLASH);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0a in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2bfc(void) {
}

/* Secondary effect function 0x0b in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2c04(void) {
}

/*
 * Secondary effect function 0x0c (Venom Trap) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 0x22, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 effect_venom_trap_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(MAIN_SFX_VENOM_TRAP);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_start_group(EFFECT_PARTICLE_GROUP_VENOM_TRAP, 12);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(EFFECT_PARTICLE_GROUP_VENOM_TRAP);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_VENOM_TRAP].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x0d (Level Up) in g_effect_secondary_handlers.
 * It shares the venom trap
 * handler's shape but seeds group 5's position_y with -8 and lowers it by 3 per
 * executing frame instead of playing a sound. Returns 1 to keep animating.
 */
s32 effect_level_up_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y = -8;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_start_group(EFFECT_PARTICLE_GROUP_LEVEL_UP, 11);
        {
            /* Addressing through a pointer keeps the target lui/addiu pair. */
            s16* rise = &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y;
            *rise -= 3;
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(EFFECT_PARTICLE_GROUP_LEVEL_UP);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0e in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2e60(void) {
}

enum {
    EFFECT_TRAP_SLOT_COUNT = 2,
    EFFECT_TRAP_CLUT = MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 15),
};

/*
 * Death Poof secondary effect (0x0f) handler for the two-slot trap group 4: it
 * starts this frame's share of the group's effect slots, steps them with a
 * gravity of (0, ONE, 0) and finishes once the spawn state's finish_timer
 * reaches 1.
 *
 * The handler table calls it with no arguments: for a phase outside 1-3 the
 * result is `passthrough`, whatever the caller left in the second argument
 * register.
 */
s32 effect_update_death_poof_secondary(s32 unused_arg0, s32 passthrough) {
    effect_secondary_data_t* effect;
    effect_death_poof_state_t* state;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s32 spawned;
    s32 i;

    effect = g_effect_current_secondary;
    result = passthrough;
    state = &g_effect_death_poof_states[effect->own_slot_id];
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING: {
        state->count = 0;
        for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
            state->ids[i] = 0;
        }
        state->finish_timer = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    }
    case EFFECT_SECONDARY_PHASE_EXECUTING: {
        effect_slot_t* slot;
        s32 i;
        u8 id;

        if (effect->timer >= g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame
            && effect->timer < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = effect_alloc_slot();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids[i]];
                slot->life = effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF], &slot->motion);
                effect_init_trap_animation(EFFECT_PARTICLE_GROUP_DEATH_POOF, EFFECT_TRAP_CLUT, slot);
                state->count++;
                if (state->count == EFFECT_TRAP_SLOT_COUNT) {
                    break;
                }
            }
        }
    }
    /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING: {
        effect_secondary_data_t* current;
        u8 id;

        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
            id = state->ids[i];
            if (id != 0 && effect_update_slot(id) == 0) {
                effect_free_slot(id);
                state->ids[i] = 0;
                state->count--;
            }
        }
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_current_secondary;
        current->timer = current->timer + 1;
        result = state->finish_timer != 1;
        if (state->count == 0
            && current->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame) {
            state->finish_timer = state->finish_timer + 1;
        }
        break;
    }
    }
    return result;
}

/* Secondary effect function 0x10 in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b3118(void) {
}

enum {
    EFFECT_TRAP3_SLOT_COUNT = 24,
};

/* Secondary effect function 0x11 (Zodiac Poof): the 24-slot particle group 3.
 *
 * Its spawn state is fixed at 0x801bade0 and its CLUT comes from the table
 * indexed by the effect's parameter, the caster's sprite palette. It follows
 * the same phase structure as the death-poof secondary handler.
 */
s32 effect_zodiac_poof_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_trap_state_24_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s32 spawned;
    s32 i;
    s32 done;
    s16 timer;
    u8 id;
    u16 clut_index;

    effect = g_effect_current_secondary;
    state = &g_effect_secondary_state.zodiac_poof;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->count = 0;
        for (spawned = 0; spawned < EFFECT_TRAP3_SLOT_COUNT; spawned++) {
            state->ids[spawned] = 0;
        }
        state->done = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        clut_index = g_effect_zodiac_poof_animations[effect->parameter];
        if (effect->timer >= g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame
            && effect->timer < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_TRAP3_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = effect_alloc_slot();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids[i]];
                slot->life = effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF], &slot->motion);
                effect_init_trap_animation(
                    EFFECT_PARTICLE_GROUP_ZODIAC_POOF, clut_index + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
                state->count++;
                if (state->count == EFFECT_TRAP3_SLOT_COUNT) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        for (spawned = 0; spawned < EFFECT_TRAP3_SLOT_COUNT; spawned++) {
            id = state->ids[spawned];
            if (id != 0 && effect_update_slot(id) == 0) {
                effect_free_slot(id);
                state->ids[spawned] = 0;
                state->count--;
            }
        }
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        {
            effect_secondary_data_t* current_effect;

            current_effect = g_effect_current_secondary;
            timer = current_effect->timer + 1;
            current_effect->timer = timer;
        }
        done = state->done;
        result = done != 1;
        if (state->count == 0
            && timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame) {
            state->done = done + 1;
        }
        break;
    }
    return result;
}

/* Secondary-effect handler 0x12 (Reflect). The effect's parameter is the
 * frames the shield stays, at least 26: the child timeline passes the
 * effect's spawn_delay.
 *
 * Phase 1 allocates and initializes the work buffer; phase 2 advances a
 * two-entry CLUT wipe near the end of the effect, uploads the CLUT, aims the
 * shield from the target's center toward the caster and hands the transform
 * to effect_draw_projectile_model; phase 3 finishes. The wipe indices are assigned inside the
 * subscript, each to its own variable: a separate statement or a folded
 * `timer - 1 - span` changes the order and association of the index
 * arithmetic. spin_timer keeps the timer load at full width (lh). */
s32 effect_reflect_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    RECT rect;
    effect_reflect_buffer_t* work;
    SVECTOR* caster;
    SVECTOR* target;
    rgb24_t* colors;
    u8* blue;
    u8* green;
    u8* red;
    s32 span;
    s32 timer;
    s32 lit;
    s32 cleared;
    s32 spin_timer;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;

    span = g_effect_current_secondary->parameter;
    if (span < 0x1a) {
        span = 0x1a;
    }
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        if (g_effect_current_secondary->allocation != 0) {
            main_heap_free(g_effect_current_secondary->allocation);
        }
        work = main_heap_alloc(0x798);
        g_effect_current_secondary->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 0x18; j++) {
                SetPolyGT3(&work->prims[i][j]);
                SetSemiTrans(&work->prims[i][j], 1);
            }
        }
        /* Separate channel pointers retain the target's three stride bases. */
        for (i = 0; i < 2; i++) {
            colors = g_effect_reflect_palette_colors;
            red = &colors->r;
            green = &colors->g;
            blue = &colors->b;
            for (j = 0; j < 6; j++) {
                work->clut[i][j] = red[j * sizeof(*colors)] | (green[j * sizeof(*colors)] << 5)
                    | (blue[j * sizeof(*colors)] << 10) | 0x8000;
            }
        }
        result = 1;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        work = g_effect_current_secondary->allocation;
        timer = g_effect_current_secondary->timer;
        if (span < timer && timer < span + 4) {
            work->clut[g_effect_buffer_index][((lit = timer - 1) - span) * 2]
                = work->clut[g_effect_buffer_index][(lit - span) * 2 + 1] = 0xffff;
        }
        if (span + 1 < g_effect_current_secondary->timer) {
            work->clut[g_effect_buffer_index][((cleared = g_effect_current_secondary->timer - 2) - span) * 2]
                = work->clut[g_effect_buffer_index][(cleared - span) * 2 + 1] = 0;
        }
        rect.x = (g_effect_current_secondary->own_slot_id - 1) * 16 + 1;
        rect.y = 0x1fc;
        rect.w = 6;
        rect.h = 1;
        LoadImage(&rect, (u32*)work->clut[g_effect_buffer_index]);
        caster = battle_unit_get_world_coords_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id);
        target = battle_unit_get_world_coords_by_runtime_id(
            g_effect_current_secondary->target.record.data.unit.runtime_id);
        setVector(&delta, caster->vx - target->vx, caster->vy - target->vy, caster->vz - target->vz);
        position.vx = target->vx;
        position.vz = target->vz;
        position.vy = target->vy
            - (s32)battle_gfx_get_unit_spritesheet_height_by_runtime_id(
                  g_effect_current_secondary->target.record.data.unit.runtime_id)
                / 2;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;
        scale.vz = 0x140;
        scale.vy = 0x140;
        scale.vx = 0x140;
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        spin.vz = 0;
        spin.vx = 0;
        spin_timer = g_effect_current_secondary->timer;
        spin.vz = spin_timer << 7;
        effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        effect = g_effect_current_secondary;
        effect->timer++;
        if (span + 4 < effect->timer) {
            effect->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x13 (Teleport) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 effect_teleport_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_current_secondary->active_count = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_start_group(EFFECT_PARTICLE_GROUP_TELEPORT, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(EFFECT_PARTICLE_GROUP_TELEPORT);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_TELEPORT].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x14 in g_effect_secondary_handlers: a
 * projectile flying in a straight line. It runs the crossbow bolt (the arrow
 * model), the thrown stone and the thrown item animations.
 *
 * Initializing measures the trajectory, takes the flight time from
 * g_effect_trajectory_step_count and allocates what the projectile draws
 * with: the polygon buffer of the arrow or stone model, or for a thrown item the
 * shuriken model's buffer (items ITEM_ID_SHURIKEN to ITEM_ID_YAGYU_DARKNESS) or
 * else an effect slot holding the item's icon as a quad. Executing moves 0x8000
 * (8 units in Q12) along the line each frame, aims the model along it and draws
 * it: the arrow at scale 0x40, the stone and the shuriken at 0xc0 and spinning by
 * the frames left, any other item as its icon sprite.
 *
 * With two frames left and a unit hit by the trajectory
 * (g_effect_trajectory_hit_unit_id), the target's hit reaction
 * (battle_unit_call_bow_hardcoding_by_runtime_id) starts, unless the item is one of
 * the balls or a consumable (ITEM_ID_POTION to ITEM_ID_PHOENIX_DOWN). When the
 * count runs out the icon slot is freed; a unit target whose result is neither
 * MISSED nor NO_EFFECT releases the requested ability effect at animation speed
 * 2, and otherwise a requested effect is dropped after the hit reaction. The
 * handler then enters Finalizing, whose return of 0 removes it.
 *
 * g_effect_projectile_spin_passes is the count of frames left. result is
 * unset for a phase outside 1-3, and scale and spin for a model other than the
 * arrow or the stone. */
s32 effect_linear_projectile_secondary_handler(void) {
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    SPRT sprite;
    effect_arrow_buffer_t* arrow;
    effect_stone_buffer_t* work12;
    effect_shuriken_buffer_t* work16;
    effect_slot_t* slot;
    s32 parameter;
    s32 item;
    s32 i;
    s32 j;
    s32 result;
    s32 kind;
    POLY_FT4* dst;

    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        setVector(&delta, g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx,
            g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy,
            g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz);
        g_effect_projectile_length
            = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
        g_effect_projectile_delta = delta;
        g_effect_projectile_progress = 0;
        g_effect_projectile_speed = 0x8000;
        g_effect_projectile_spin_passes = g_effect_trajectory_step_count;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        if (g_effect_current_secondary->allocation != 0) {
            main_heap_free(g_effect_current_secondary->allocation);
        }
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        if ((parameter & 0xff) == EFFECT_MODEL_ARROW) {
            arrow = main_heap_alloc(0xc88);
            g_effect_current_secondary->allocation = arrow;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 4; j++) {
                    SetPolyG3(&arrow->g3[i][j]);
                }
                for (j = 0; j < 13; j++) {
                    SetPolyG4(&arrow->g4[i][j]);
                }
            }
        }
        if ((parameter & 0xff) == EFFECT_MODEL_STONE) {
            work12 = main_heap_alloc(0x2a0);
            g_effect_current_secondary->allocation = work12;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 12; j++) {
                    SetPolyG3(&work12->g3[i][j]);
                }
            }
        }
        if (parameter == EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            item = g_effect_current_secondary->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                work16 = main_heap_alloc(0x380);
                g_effect_current_secondary->allocation = work16;
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 16; j++) {
                        SetPolyG3(&work16->g3[i][j]);
                    }
                }
            } else {
                gfx_set_item_icon_sprite(&sprite, item);
                g_effect_current_secondary->slot_ids[0] = effect_alloc_slot();
                slot = &g_effect_slots[g_effect_current_secondary->slot_ids[0]];
                dst = &slot->quads[1];
                SetPolyFT4(&slot->quads[0]);
                slot->quads[0].tpage = 0x1e;
                slot->quads[0].b0 = 0x80;
                slot->quads[0].g0 = 0x80;
                slot->quads[0].r0 = 0x80;
                slot->quads[0].u0 = slot->quads[0].u2 = sprite.u0;
                slot->quads[0].u1 = slot->quads[0].u3 = slot->quads[0].u0 + sprite.w;
                slot->quads[0].v0 = slot->quads[0].v1 = sprite.v0;
                slot->quads[0].v2 = slot->quads[0].v3 = slot->quads[0].v0 + sprite.h;
                slot->quads[0].clut = sprite.clut;
                *dst = slot->quads[0];
            }
        }
        battle_state_set_animation_speed(1);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_effect_projectile_progress += g_effect_projectile_speed;
        effect_find_projectile_xyz_along_trajectory(
            &g_effect_projectile_delta, g_effect_projectile_length, g_effect_projectile_progress, &position);
        setVector(&position, g_effect_trajectory_origin.vx + position.vx, g_effect_trajectory_origin.vy + position.vy,
            g_effect_trajectory_origin.vz + position.vz);
        delta.vx = g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx;
        delta.vz = g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz;
        delta.vy = g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        i = parameter & 0xff;
        if (parameter != EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            if (i == EFFECT_MODEL_ARROW) {
                scale.vz = 0x40;
                scale.vy = 0x40;
                scale.vx = 0x40;
                spin.vz = 0;
                spin.vy = 0;
                spin.vx = 0;
            } else if (i == EFFECT_MODEL_STONE) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                setVector(&spin, 0, g_effect_projectile_spin_passes * 256, g_effect_projectile_spin_passes * 128);
            }
            effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        } else {
            item = g_effect_current_secondary->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                spin.vy = 0;
                spin.vx = 0;
                spin.vz = -(g_effect_projectile_spin_passes * 256);
                effect_draw_projectile_model(&spin, &position, &angles, &scale, EFFECT_MODEL_SHURIKEN);
            } else {
                effect_draw_item_drop_sprite(&position, item, 0);
            }
        }
        if (g_effect_projectile_spin_passes == 2 && g_effect_trajectory_hit_unit_id != -1) {
            kind = g_effect_current_secondary->used_weapon_id;
            if ((kind < ITEM_ID_FIRE_BALL || kind > ITEM_ID_LIGHTNING_BALL)
                && (kind < ITEM_ID_POTION || kind > ITEM_ID_PHOENIX_DOWN)
                && g_effect_current_secondary->target_count != 0) {
                battle_unit_call_bow_hardcoding_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id,
                    g_effect_current_secondary->target.record.data.unit.runtime_id);
            }
        }
        if (g_effect_projectile_spin_passes > 0) {
            g_effect_projectile_spin_passes--;
        }
        if (g_effect_projectile_spin_passes == 0) {
            if (g_effect_current_secondary->slot_ids[0] != 0) {
                effect_free_slot(g_effect_current_secondary->slot_ids[0]);
                g_effect_current_secondary->slot_ids[0] = 0;
            }
            if (g_effect_current_secondary->target.record.target_type == EFFECT_TARGET_TYPE_UNIT
                && g_effect_current_secondary->target.record.data.unit.result_animation != EFFECT_HIT_MISSED
                && g_effect_current_secondary->target.record.data.unit.result_animation != EFFECT_HIT_NO_EFFECT) {
                effect_play();
                battle_state_set_animation_speed(2);
            } else if (effect_get_playback_phase() == EFFECT_PLAYBACK_PHASE_REQUESTED) {
                if (g_effect_current_secondary->target_count != 0) {
                    battle_unit_call_bow_hardcoding_by_runtime_id(
                        g_effect_current_secondary->caster.data.unit.runtime_id,
                        g_effect_current_secondary->target.record.data.unit.runtime_id);
                }
                effect_cancel_playback_request();
            }
            g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x15 (Equip Break) in g_effect_secondary_handlers.
 * Same shape as the venom trap
 * handler, but its particle group is held in a variable, so its spawn_start_frame lookup
 * is an indexed table read. Returns 1 to keep animating.
 */
s32 effect_equip_break_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    vec3s32_t gravity;
    s32 group;
    s32 result;
    s16 finish_timer;

    group = EFFECT_PARTICLE_GROUP_EQUIP_BREAK;
    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_start_group(group, 10);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_update_active_slots(group);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0 && current->timer > g_effect_trap_particle_configs[group].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

enum {
    EFFECT_RING_CLUT = MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 12),
    EFFECT_RING_SLOT_COUNT = 30,
};

/* Secondary effect 0x16 (summon charge): three rings of ten effect slots
 * spinning around the target.
 *
 * Initialization allocates the 30
 * slots and plays sound 0x21; the executing phase grows the radius, spin and
 * brightness with the timer, the finalizing phase widens the rings and fades
 * them out over finish_timer, and both then advance the ring angle, record each
 * ring's newest point and place every slot at a trailing point. Once finish_timer
 * passes 60 the slots are freed and sound 0x21 is paused. Returns 1 while the
 * effect runs.
 */
s32 effect_summon_charge_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_ring_state_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x68-byte frame. */
    s32 result;
    s32 i;
    s32 j;
    s32 k;
    u8 id;

    effect = g_effect_current_secondary;
    state = &g_effect_secondary_state.summon_ring;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->index = 0;
        state->radius = 0;
        state->angle_step = 0;
        state->angle = 0;
        effect_resolve_target_world_coords(0, EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &state->center);
        state->brightness = 0;
        state->center.vy -= 24;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                state->points[i][j].vx = state->points[i][j].vy = state->points[i][j].vz = 0;
            }
        }
        for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
            j = g_effect_current_secondary->slot_ids[i] = effect_alloc_slot();
            if (j != 0) {
                slot = &g_effect_slots[j];
                slot->life = effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SUMMON_CHARGE], &slot->motion);
                effect_init_trap_animation(EFFECT_PARTICLE_GROUP_SUMMON_CHARGE, EFFECT_RING_CLUT, slot);
            }
        }
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(MAIN_SFX_SUMMON_CHARGE);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        if (effect->timer < 24) {
            state->radius = effect->timer;
            state->angle_step = effect->timer * 3;
        }
        if (effect->timer < 8) {
            state->brightness = (effect->timer + 1) * 16;
        }
        if (effect->timer < 256) {
            effect->timer++;
        }
        goto update; /* skip the finalizing fade into the shared update */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        if (effect->finish_timer < 61) {
            if (effect->finish_timer & 8) {
                state->radius++;
            }
            state->angle_step++;
            if (effect->finish_timer < 61) {
                state->brightness = (60 - effect->finish_timer) * 4;
                if (effect->finish_timer < 61) {
                    effect->finish_timer++;
                }
            }
        }
    update:
        state->angle += state->angle_step;
        for (i = 0; i < 3; i++) {
            state->points[i][state->index].vx = rcos(i * 0x555 + state->angle) * state->radius / ONE;
            state->points[i][state->index].vz = rsin(i * 0x555 + state->angle) * state->radius / ONE;
        }
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                id = g_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->color.r = slot->color.g = slot->color.b
                        = g_effect_summon_ring_brightness[j] * state->brightness / 128;
                }
            }
        }
        for (i = 0; i < 3; i++) {
            k = state->index;
            for (j = 0; j < 10; j++) {
                id = g_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->motion.position.x = (state->center.vx + state->points[i][k].vx) * ONE;
                    slot->motion.position.y = (state->center.vy + state->points[i][k].vy) * ONE;
                    slot->motion.position.z = (state->center.vz + state->points[i][k].vz) * ONE;
                    effect_update_slot(g_effect_current_secondary->slot_ids[i * 10 + j]);
                }
                k++;
                if (k == 10) {
                    k = 0;
                }
            }
        }
        state->index++;
        if (state->index >= 10) {
            state->index = 0;
        }
        if (g_effect_current_secondary->finish_timer >= 61) {
            for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
                if (g_effect_current_secondary->slot_ids[i] != 0) {
                    effect_free_slot(g_effect_current_secondary->slot_ids[i]);
                    g_effect_current_secondary->slot_ids[i] = 0;
                }
            }
            suzuki_sound_stop_sfx_channels(MAIN_SFX_SUMMON_CHARGE);
            result = 0;
        } else {
            result = 1;
        }
        break;
    }
    return result;
}

/* Run the handler of every active secondary effect once, newest first, and remove
 * each one whose handler returns 0. Returns 0 when none is active.
 *
 * Before the handlers run it takes this frame's ordering table and flips the
 * buffer index their double-buffered primitives use. */
s32 effect_update_secondary_effects(void) {
    effect_secondary_data_t* effect;
    u32 previous_slot_id;

    g_effect_polygon_depth_data = main_gfx_get_otag();
    g_effect_buffer_index = 1 - g_effect_buffer_index;
    g_effect_current_slot_id = g_effect_secondary_active_tail;
    if (g_effect_secondary_active_tail == 0) {
        return 0;
    }

    do {
        effect = &g_effect_secondary_data[g_effect_current_slot_id];
        g_effect_current_secondary = effect;
        previous_slot_id = effect->previous_slot_id;
        if (g_effect_secondary_handlers[effect->function_id]() == 0) {
            effect_remove_secondary(g_effect_current_slot_id);
        }
        g_effect_current_slot_id = previous_slot_id;
    } while (previous_slot_id != 0);

    return 1;
}
