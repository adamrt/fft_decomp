/* Effect secondary system and camera. Its start is a boundary the binary proves; its end is a cut chosen by topic. */

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

void effect_convert_tile_coords_to_world_coords(SVECTOR* in, VECTOR* out) {
    map_tile_t* tile;

    tile = map_get_tile_data_pointer(in->vx, in->vz, in->vy);
    out->vx = in->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    out->vz = in->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    out->vy = -(tile->height * 12);
}

/* Copies the camera rotation, snapped to the nearest yaw quadrant whose view
 * of the tile is not blocked. The camera angles are read as an SVECTOR: the
 * quadrant is a signed short division, which unsigned 16-bit values do not emit. */
void camera_choose_unblocked_rotation_for_tile(const SVECTOR* tile_coords, SVECTOR* rotation_out) {
    map_tile_t* tile;
    SVECTOR* rotation;
    s32 mask;
    s32 quadrant;
    s32 yaw;

    tile = map_get_tile_data_pointer(tile_coords->vx, tile_coords->vz, tile_coords->vy);
    mask = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    rotation = camera_get_rotation();
    rotation_out->vx = rotation->vx;
    rotation_out->vz = rotation->vz;
    quadrant = (rotation->vy & 0xfff) / 0x400;
    yaw = rotation->vy & 0xe00;
    if ((g_camera_quadrant_block_directions[quadrant] & mask) == 0 || mask == MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK) {
        rotation_out->vy = yaw;
        return;
    }
    if ((g_camera_quadrant_block_directions[(quadrant + 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw + 0x400;
    } else if ((g_camera_quadrant_block_directions[(quadrant - 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw - 0x400;
    } else {
        rotation_out->vy = (yaw + 0x800) & 0xfff;
    }
}

/* The 12-byte target stores both arguments to their incoming stack home slots
 * and returns. Taking their addresses reproduces those stores; it does not
 * establish what the original source body contained. */
void battle_noop_801aad30(s32 ignored_1, s32 ignored_2) {
    s32* unused;
    unused = &ignored_1;
    unused = &ignored_2;
}

/* A tile is 28 units wide, so max * 14 is the middle of the map. */
/* Writes the map's center as three 32-bit X/Z/Y components with Z (height) zero
 * (effect camera target). */
void effect_store_map_center_coordinates(VECTOR* destination) {
    SVECTOR map_max;
    s32 max_y;

    map_store_max_coordinates(&map_max);
    destination->vx = map_max.vx * (MAP_TILE_SIZE / 2);
    max_y = map_max.vz;
    destination->vy = 0;
    destination->vz = max_y * (MAP_TILE_SIZE / 2);
}

/* Timeline track 1 handler: sets the effect camera angle target and duration.
 *
 * Mode bits 0x1e0 pick the source: the best unblocked view of a target's or
 * the caster's tile (0x0, 0x140) or of the cursor tile (0x1c0), the given
 * angles with the yaw wrapped to the nearest turn (0x40), the current
 * rotation (0xc0), the current rotation plus the given offsets (0x20), the
 * saved pitch with the current yaw/roll (0x60) or the saved angles with the yaw
 * wrapped against g_effect_camera_rotation_current (0x100). Most modes then add `angles` when
 * present. Bits 0x1e00 are kept in g_effect_camera_rotation_mode; other modes return untouched.
 *
 * Mode 0x40 keeps its own `current` pointer: sharing `rotation` with the other
 * modes changes that case's register allocation. */
void camera_execute_angle_command(s32 command, s32 target, s32 duration, const SVECTOR* angles) {
    SVECTOR coords;
    SVECTOR* rotation;
    SVECTOR* current;
    s16 delta;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            battle_unit_get_map_coords_by_runtime_id(g_effect_context.targets[target].data.unit.runtime_id, &coords);
        } else {
            coords = g_effect_context.targets[target].data.tile;
        }
        camera_choose_unblocked_rotation_for_tile(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        current = camera_get_rotation();
        delta = angles->vy - current->vy;
        if (delta < -0x800) {
            delta += ONE;
        } else if (delta > 0x800) {
            delta -= ONE;
        }
        setVector(&g_effect_camera_rotation_target, angles->vx, delta + current->vy, angles->vz);
        g_effect_camera_rotation_mode = command & EFFECT_CAMERA_MODE_MASK;
        g_effect_camera_rotation_duration = duration;
        g_effect_camera_rotation_frame = 0;
        return;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        rotation = camera_get_rotation();
        copyVector(&g_effect_camera_rotation_target, rotation);
        if (angles) {
            effect_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_rotation_target = g_effect_camera_rotation_saved;
        if (angles) {
            effect_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        delta = g_effect_camera_rotation_target.vy - g_effect_camera_rotation_current.vy;
        if (delta > 0x800) {
            g_effect_camera_rotation_target.vy -= ONE;
        }
        if (delta < -0x800) {
            g_effect_camera_rotation_target.vy += ONE;
        }
        break;
    case EFFECT_CAMERA_SOURCE_CASTER:
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            battle_unit_get_map_coords_by_runtime_id(
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id, &coords);
        } else {
            coords = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile;
        }
        camera_choose_unblocked_rotation_for_tile(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        coords = g_effect_target_tile.data.tile;
        camera_choose_unblocked_rotation_for_tile(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT_YAW_ROLL:
        rotation = camera_get_rotation();
        if (angles) {
            setVector(
                &g_effect_camera_rotation_target, angles->vx, rotation->vy + angles->vy, rotation->vz + angles->vz);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED_PITCH:
        rotation = camera_get_rotation();
        setVector(&g_effect_camera_rotation_target, g_effect_camera_rotation_saved.vx, rotation->vy, rotation->vz);
        if (angles) {
            effect_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    default:
        return;
    }
    g_effect_camera_rotation_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_effect_camera_rotation_duration = duration;
    g_effect_camera_rotation_frame = 0;
}

/* Timeline track 2 handler: sets the effect camera position target and duration.
 *
 * Mode bits 0x1e0 pick the source: effect target `target` (0x0, which also
 * saves the current camera position to g_effect_camera_position_start), the given vector (0x40),
 * the map center (0x80), the current camera (0xc0), the saved position at
 * g_effect_camera_position_saved (0x100), the caster entry (0x140), the average of all unit
 * targets (0x180) or the tile at g_effect_target_tile (0x1c0). All but 0x40 add offset
 * when present. Bits 0x1e00 are kept in g_effect_camera_position_mode. */
void camera_execute_position_command(s32 command, s32 target, s32 duration, const VECTOR* offset) {
    VECTOR sum;
    VECTOR* coords;
    SVECTOR* screen;
    s32 i;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_copy_unit_world_coords_to_vector(
                g_effect_context.targets[target].data.unit.runtime_id, &g_effect_camera_position_target);
        } else {
            effect_convert_tile_coords_to_world_coords(
                &g_effect_context.targets[target].data.tile, &g_effect_camera_position_target);
        }
        coords = camera_get_current_real_coords();
        effect_shift_vector_right_12(coords, &g_effect_camera_position_start);
        if (offset) {
            effect_add_vectors(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        g_effect_camera_position_target = *offset;
        break;
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
        effect_store_map_center_coordinates(&g_effect_camera_position_target);
        if (offset) {
            effect_add_vectors(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        coords = camera_get_current_real_coords();
        setVector(&g_effect_camera_position_target, coords->vx >> 12, coords->vy >> 12, coords->vz >> 12);
        if (offset) {
            effect_add_vectors(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_position_target = g_effect_camera_position_saved;
        if (offset) {
            effect_add_vectors(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CASTER:
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_copy_unit_world_coords_to_vector(
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id, &g_effect_camera_position_target);
        } else {
            effect_convert_tile_coords_to_world_coords(
                &g_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile, &g_effect_camera_position_target);
        }
        if (offset) {
            effect_add_vectors(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        setVector(&sum, 0, 0, 0);
        for (i = 0; i < g_effect_context.target_count; i++) {
            if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                screen = battle_unit_get_world_coords_by_runtime_id(g_effect_context.targets[i].data.unit.runtime_id);
                sum.vx += screen->vx;
                sum.vy += screen->vy;
                sum.vz += screen->vz;
            }
        }
        sum.vx /= i;
        sum.vy /= i;
        sum.vz /= i;
        if (offset) {
            effect_add_vectors(offset, &sum, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        effect_convert_tile_coords_to_world_coords(&g_effect_target_tile.data.tile, &g_effect_camera_position_target);
        if (offset) {
            effect_add_vectors(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    default:
        return;
    }
    g_effect_camera_position_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_effect_camera_position_duration = duration;
    g_effect_camera_position_frame = 0;
}

/* Timeline track 3 handler: sets the effect camera zoom target and duration.
 *
 * Mode bits 0x1e0 pick the source: the given vector, the current camera zoom
 * (0xc0) or the saved zoom at g_effect_camera_zoom_saved (0x100), the latter two offset by
 * zoom when present. Other modes leave the camera untouched. Bits 0x1e00 are
 * kept in g_effect_camera_zoom_mode. */
void camera_execute_zoom_command(s32 command, s32 unused, s32 duration, const VECTOR* zoom) {
    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
    case EFFECT_CAMERA_SOURCE_GIVEN:
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
    case EFFECT_CAMERA_SOURCE_CASTER:
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        g_effect_camera_zoom_target = *zoom;
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        g_effect_camera_zoom_target = *camera_get_zoom();
        if (zoom) {
            effect_add_vectors(zoom, &g_effect_camera_zoom_target, &g_effect_camera_zoom_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_zoom_target = g_effect_camera_zoom_saved;
        if (zoom) {
            effect_add_vectors(zoom, &g_effect_camera_zoom_target, &g_effect_camera_zoom_target);
        }
        break;
    default:
        return;
    }
    g_effect_camera_zoom_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_effect_camera_zoom_duration = duration;
    g_effect_camera_zoom_frame = 0;
}

/* Advance the effect camera rotation, position and zoom tracks one frame.
 *
 * Each track's 32-bit mode (g_effect_camera_rotation_mode, _position_mode and _zoom_mode: bits 0x1e00 of the
 * timeline command) selects an interpolation between the start and target values over the track duration; the mode
 * clears once the counter reaches the duration. */
void camera_update_effect_rotation_and_zoom(void) {
    VECTOR position;
    VECTOR delta;
    SVECTOR rotation;
    SVECTOR rotation_delta;

    switch (g_effect_camera_rotation_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        camera_set_rotation(&g_effect_camera_rotation_target);
        g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_rotation_start.vx = g_effect_camera_rotation_current.vx
            = g_effect_camera_rotation_target.vx & 0xfff;
        g_effect_camera_rotation_start.vy = g_effect_camera_rotation_current.vy
            = g_effect_camera_rotation_target.vy & 0xfff;
        g_effect_camera_rotation_start.vz = g_effect_camera_rotation_current.vz
            = g_effect_camera_rotation_target.vz & 0xfff;
        g_effect_camera_rotation_frame = g_effect_camera_rotation_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_interpolate_svector_cos(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_set_rotation(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            effect_mask_vector_low_12_bits(&g_effect_camera_rotation_target, &g_effect_camera_rotation_start);
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            camera_set_rotation(&g_effect_camera_rotation_start);
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_Q12:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_interpolate_svector_cos_q12(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_set_rotation(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_interpolate_svector_linear(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_set_rotation(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            effect_mask_vector_low_12_bits(&g_effect_camera_rotation_target, &g_effect_camera_rotation_start);
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            camera_set_rotation(&g_effect_camera_rotation_start);
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&rotation, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_add_rotation_vectors_mod_q12(
                &g_effect_camera_rotation_current, &rotation_delta, &g_effect_camera_rotation_current);
            camera_set_rotation(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_target = g_effect_camera_rotation_current;
            g_effect_camera_rotation_start = g_effect_camera_rotation_target;
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_add_rotation_vectors_mod_q12(
                &g_effect_camera_rotation_current, &g_effect_camera_rotation_target, &g_effect_camera_rotation_current);
            camera_set_rotation(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_target = g_effect_camera_rotation_current;
            g_effect_camera_rotation_start = g_effect_camera_rotation_target;
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&g_effect_camera_rotation_target, &rotation,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_add_rotation_vectors_mod_q12(
                &g_effect_camera_rotation_current, &rotation_delta, &g_effect_camera_rotation_current);
            camera_set_rotation(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_target = g_effect_camera_rotation_current;
            g_effect_camera_rotation_start = g_effect_camera_rotation_target;
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&rotation, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_add_random_rotation_offsets(&g_effect_camera_rotation_start, &rotation_delta, &rotation);
            camera_set_rotation(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_add_random_rotation_offsets(
                &g_effect_camera_rotation_start, &g_effect_camera_rotation_target, &rotation);
            camera_set_rotation(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&g_effect_camera_rotation_target, &rotation,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_add_random_rotation_offsets(&g_effect_camera_rotation_start, &rotation_delta, &rotation);
            camera_set_rotation(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    switch (g_effect_camera_position_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        effect_scale_vector_fixed12(&g_effect_camera_position_target, &position);
        camera_set_current_real_coords(&position);
        g_effect_camera_position_current = g_effect_camera_position_target;
        g_effect_camera_position_start = g_effect_camera_position_current;
        g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_position_frame = g_effect_camera_position_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_interpolate_vector_q13_cos(&g_effect_camera_position_start, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &g_effect_camera_position_current);
            effect_scale_vector_fixed12(&g_effect_camera_position_current, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_Q12:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_interpolate_vector_q13_cos_q12(&g_effect_camera_position_start, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &g_effect_camera_position_current);
            effect_scale_vector_fixed12(&g_effect_camera_position_current, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_interpolate_vector_q13_linear(&g_effect_camera_position_start, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &g_effect_camera_position_current);
            effect_scale_vector_fixed12(&g_effect_camera_position_current, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_add_vector_and_store_q12(&delta, &g_effect_camera_position_current, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_target = g_effect_camera_position_current;
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_add_vector_and_store_q12(
                &g_effect_camera_position_target, &g_effect_camera_position_current, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_target = g_effect_camera_position_current;
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_position_target, &position,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_add_vector_and_store_q12(&delta, &g_effect_camera_position_current, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_target = g_effect_camera_position_current;
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_add_random_vector_offsets_and_store_q12(&g_effect_camera_position_start, &delta, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_add_random_vector_offsets_and_store_q12(
                &g_effect_camera_position_start, &g_effect_camera_position_target, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_position_target, &position,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_add_random_vector_offsets_and_store_q12(&g_effect_camera_position_start, &delta, &position);
            camera_set_current_real_coords(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    switch (g_effect_camera_zoom_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        camera_set_zoom(&g_effect_camera_zoom_target);
        g_effect_camera_zoom_current = g_effect_camera_zoom_target;
        g_effect_camera_zoom_start = g_effect_camera_zoom_current;
        g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_zoom_frame = g_effect_camera_zoom_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_interpolate_vector_q13_cos(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_set_zoom(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_Q12:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_interpolate_vector_q13_cos_q12(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_set_zoom(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_interpolate_vector_q13_linear(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_set_zoom(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_zoom_target, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_add_vectors(&delta, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_set_zoom(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_target = g_effect_camera_zoom_current;
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_add_vectors(
                &g_effect_camera_zoom_target, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_set_zoom(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_target = g_effect_camera_zoom_current;
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_zoom_target, &position, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_add_vectors(&delta, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_set_zoom(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_target = g_effect_camera_zoom_current;
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_zoom_target, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_add_random_vector_offsets(&g_effect_camera_zoom_start, &delta, &position);
            camera_set_zoom(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_add_random_vector_offsets(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target, &position);
            camera_set_zoom(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_zoom_target, &position, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_add_random_vector_offsets(&g_effect_camera_zoom_start, &delta, &position);
            camera_set_zoom(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }
}

s32 effect_find_next_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    effect_timeline_t* data;
    s32 i;

    data = g_effect_timeline;
    if (frame < data->child.duration) {
        i = 0;
        if (data->last_keyframe_index >= 0) {
            do {
                if (frame < data->end_frame[i] && (mask & data->command[i]) != 0) {
                    *out_index = i;
                    *out_frame = g_effect_timeline->end_frame[i];
                    return 1;
                }
                i++;
                data = g_effect_timeline;
            } while (i <= data->last_keyframe_index);
        }
    }
    return 0;
}

/* Find the first camera keyframe of the tracks in mask that is still ahead
 * of frame, reporting (table << 16 | index) and (target << 16 | end frame).
 *
 * The phase 1 table covers frames before phase1_duration; the child table
 * repeats every spawn_delay frames per effect target (current target, then
 * the next one), and once more for the last target within child_duration; the
 * phase 2 table is relative to the end of that window.
 *
 * The conditional-expression header reads keep the entry block from caching
 * the unit-data pointer. */
s32 camera_find_active_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    s32 phase1;
    s32 spawn_delay;
    s32 child_duration;
    s32 i;
    s32 target_index;
    s32 local_frame;
    s32 elapsed;
    s32 start;

    phase1 = g_effect_timeline->phase1_duration;
    spawn_delay = g_effect_context.target_count != 0 ? g_effect_timeline->spawn_delay : 0;
    child_duration = g_effect_context.target_count != 0 ? g_effect_timeline->child.duration : 0;

    if (frame < phase1) {
        for (i = 0; i <= g_effect_timeline->phase1_camera.last_keyframe_index; i++) {
            if (frame < g_effect_timeline->phase1_camera.end_frame[i]
                && (mask & g_effect_timeline->phase1_camera.command[i]) != 0) {
                *out_index = i;
                *out_frame = g_effect_timeline->phase1_camera.end_frame[i];
                return 1;
            }
        }
    }

    if (spawn_delay != 0 && frame < phase1 + (g_effect_context.target_count - 1) * spawn_delay) {
        target_index = 0;
        if (frame >= phase1) {
            elapsed = frame - phase1;
            target_index = elapsed / spawn_delay;
            local_frame = elapsed - target_index * spawn_delay;
        } else {
            local_frame = -1;
        }
        for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
            if (g_effect_timeline->end_frame[i] >= spawn_delay) {
                break;
            }
            if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                *out_frame
                    = (target_index << 16) + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                return 1;
            }
        }
        target_index++;
        local_frame = -1;
        if (target_index < g_effect_context.target_count) {
            for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
                if (g_effect_timeline->end_frame[i] >= spawn_delay) {
                    break;
                }
                if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (g_effect_context.target_count != 0) {
        target_index = g_effect_context.target_count - 1;
        start = phase1 + target_index * spawn_delay;
        if (frame < start + child_duration) {
            local_frame = -1;
            if (frame >= start) {
                local_frame = frame - phase1 - target_index * spawn_delay;
            }
            for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
                if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (frame >= phase1 + (g_effect_context.target_count - 1) * spawn_delay + child_duration) {
        local_frame = frame - phase1 - (g_effect_context.target_count - 1) * spawn_delay - child_duration;
    } else {
        local_frame = -1;
    }
    for (i = 0; i <= g_effect_timeline->phase2_camera.last_keyframe_index; i++) {
        if (local_frame < g_effect_timeline->phase2_camera.end_frame[i]
            && (mask & g_effect_timeline->phase2_camera.command[i]) != 0) {
            *out_index = i + (EFFECT_CAMERA_TABLE_PHASE2 << 16);
            *out_frame = g_effect_timeline->phase2_camera.end_frame[i] + phase1
                + (g_effect_context.target_count - 1) * spawn_delay + child_duration;
            return 1;
        }
    }
    return 0;
}

/* Starts the child timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the child camera table. */
void camera_start_effect_keyframe_moves(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        effect_load_svector(&g_effect_timeline->rotation[index], &rotation);
        camera_execute_angle_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &rotation);
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        effect_load_vector(&g_effect_timeline->position[index], &vector);
        camera_execute_position_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        effect_load_vector(&g_effect_timeline->zoom[index], &vector);
        camera_execute_zoom_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
}

/* Starts the parent timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the table camera_find_active_keyframe reports (phase 1, child
 * or phase 2). The three-table form of
 * camera_start_effect_keyframe_moves. */
void camera_advance_effect_tracks(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;
    s32 k;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_load_svector(&g_effect_timeline->phase1_camera.rotation[k], &rotation);
            camera_execute_angle_command(g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_load_svector(&g_effect_timeline->rotation[k], &rotation);
            camera_execute_angle_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_load_svector(&g_effect_timeline->phase2_camera.rotation[k], &rotation);
            camera_execute_angle_command(g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        }
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_load_vector(&g_effect_timeline->phase1_camera.position[k], &vector);
            camera_execute_position_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_load_vector(&g_effect_timeline->position[k], &vector);
            camera_execute_position_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_load_vector(&g_effect_timeline->phase2_camera.position[k], &vector);
            camera_execute_position_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_load_vector(&g_effect_timeline->phase1_camera.zoom[k], &vector);
            camera_execute_zoom_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_load_vector(&g_effect_timeline->zoom[k], &vector);
            camera_execute_zoom_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_load_vector(&g_effect_timeline->phase2_camera.zoom[k], &vector);
            camera_execute_zoom_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
}

void effect_init_system(void) {
    s32 i;
    s32 j;

    g_effect_slots[0].prev = 0;
    g_effect_slots[0].next = 0;
    g_effect_slot_free_head = 1;
    for (i = 1; i < 32; i++) {
        g_effect_slots[i].next = i + 1;
        g_effect_slots[i].prev = i - 1;
    }
    g_effect_slots[i].next = 0;
    g_effect_slots[i].prev = i - 1;
    g_effect_active_slot_head = 0;
    g_effect_projectile_matrix.t[2] = 0;
    g_effect_projectile_matrix.t[1] = 0;
    g_effect_projectile_matrix.t[0] = 0;
    for (i = 1; i < EFFECT_SECONDARY_SLOT_COUNT; i++) {
        g_effect_secondary_data[i].own_slot_id = i;
        g_effect_secondary_data[i].previous_slot_id = i + 1;
        g_effect_secondary_data[i].next_slot_id = i - 1;
        g_effect_secondary_data[i].function_id = 0;
        g_effect_secondary_data[i].allocation = 0;
    }
    g_effect_secondary_data[0].previous_slot_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].next_slot_id = EFFECT_SECONDARY_SLOT_COUNT - 1;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].own_slot_id = EFFECT_SECONDARY_SLOT_COUNT;
    g_effect_secondary_free_head = 1;
    g_effect_secondary_data[0].next_slot_id = 0;
    g_effect_secondary_data[0].own_slot_id = 0;
    g_effect_secondary_data[0].function_id = 0;
    g_effect_secondary_data[0].allocation = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].previous_slot_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].function_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].allocation = 0;
    g_effect_secondary_active_tail = 0;
    g_effect_trap_frame_data_ptr = &g_effect_trap_frame_data;
    for (i = 0; i < EFFECT_SECONDARY_RECORD_COUNT; i++) {
        g_effect_secondary_data[i].active_count = 0;
        for (j = 31; j >= 0; j--) {
            g_effect_secondary_data[i].slot_ids[j] = 0;
        }
    }
    effect_reset_render_state();
    effect_init_camera_and_screen_data();
}

/* Take a secondary effect record from the free chain and append it to the active
 * chain, with its timer and slot ids cleared, and return its id. When no record
 * is free it passes code 0x66 to the empty report hook and returns 0.
 *
 * The id is kept as an s32 byte load and a separate `slot = id & 0xff` indexes
 * the table: the target tests and indexes through the masked copy ($v0) while
 * storing and returning the raw id ($s0), which keeps the error path's `move
 * v0,s0` instead of a folded 0. Writing previous_slot_id before the next_slot_id
 * clear reproduces the target's early g_effect_secondary_active_tail load. */
u8 effect_allocate_secondary_slot(void) {
    s32 id;
    s32 slot;
    s32 i;

    id = (u8)g_effect_secondary_free_head;
    slot = id & 0xff;
    if (slot != 0) {
        g_effect_secondary_free_head = g_effect_secondary_data[slot].previous_slot_id;
        g_effect_secondary_data[g_effect_secondary_free_head].next_slot_id = 0;
        g_effect_secondary_data[slot].previous_slot_id = g_effect_secondary_active_tail;
        g_effect_secondary_data[slot].next_slot_id = 0;
        g_effect_secondary_data[g_effect_secondary_active_tail].next_slot_id = id;
        g_effect_secondary_active_tail = id;
        g_effect_secondary_data[slot].timer = 0;
        for (i = 31; i >= 0; i--) {
            g_effect_secondary_data[slot].slot_ids[i] = 0;
        }
        return id;
    }
    main_system_report_stub(1, 0x66);
    return id;
}

/* Take secondary effect record id (1-16) off the active chain, free its work
 * block and append it to the end of the free chain headed by
 * g_effect_secondary_free_head: the inverse of
 * effect_allocate_secondary_slot. An id outside 1-16 only reaches the
 * empty report hook with code 0x67.
 *
 * Both chains are walked through previous_slot_id. A record removed from the
 * middle of the active chain passes its previous link to the record after it, but
 * the record before it keeps next_slot_id pointing at the freed record; only the
 * tail's next link is cleared.
 *
 * The free-chain walk index is an s32: a u16 adds an `andi 0xffff` per load. */
void effect_remove_secondary(u8 id) {
    effect_secondary_data_t* slot;
    s32 tail;

    slot = &g_effect_secondary_data[id];
    if (id != 0 && id < EFFECT_SECONDARY_RECORD_COUNT) {
        if (slot->allocation != 0) {
            main_heap_free(slot->allocation);
            slot->allocation = 0;
        }
        if (slot->next_slot_id != 0) {
            g_effect_secondary_data[slot->next_slot_id].previous_slot_id = slot->previous_slot_id;
        } else {
            g_effect_secondary_active_tail = slot->previous_slot_id;
        }
        g_effect_secondary_data[g_effect_secondary_active_tail].next_slot_id = 0;
        tail = g_effect_secondary_free_head;
        if (tail != 0) {
            while (g_effect_secondary_data[tail].previous_slot_id != 0) {
                tail = g_effect_secondary_data[tail].previous_slot_id;
            }
            g_effect_secondary_data[tail].previous_slot_id = id;
            slot->next_slot_id = tail;
        } else {
            g_effect_secondary_free_head = id;
            slot->next_slot_id = 0;
        }
        slot->previous_slot_id = 0;
        slot->phase = EFFECT_SECONDARY_PHASE_ENDED;
        return;
    }
    main_system_report_stub(1, 0x67);
}

effect_secondary_phase_e effect_get_secondary_phase(u8 secondary_id) {
    return g_effect_secondary_data[secondary_id].phase;
}

void effect_finalize_secondary(u8 secondary_id) {
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_FINALIZING;
}

/* Take an effect slot from the free chain and push it on the head of the active
 * chain. Returns its id, or 0 (the null sentinel) when no slot is free; the
 * slot's life is cleared either way. */
s32 effect_alloc_slot(void) {
    u8 head;
    u8 tail;
    s32 id;

    head = g_effect_slot_free_head;
    id = head;
    if (id != 0) {
        g_effect_slot_free_head = g_effect_slots[id].next;
        g_effect_slots[g_effect_slots[id].next].prev = 0;
        g_effect_slots[g_effect_active_slot_head].prev = head;
        tail = g_effect_active_slot_head;
        g_effect_slots[id].prev = 0;
        g_effect_slots[id].next = tail;
        g_effect_active_slot_head = head;
    }
    g_effect_slots[id].life = 0;
    return id;
}

/* Unlinks slot id from the active chain and pushes it onto the free chain;
 * slot 0 is the null sentinel and is ignored. */
void effect_free_slot(u8 id) {
    u8 free_head;

    if (id == 0) {
        return;
    }
    g_effect_slots[g_effect_slot_free_head].prev = id;
    if (g_effect_slots[id].prev != 0) {
        g_effect_slots[g_effect_slots[id].prev].next = g_effect_slots[id].next;
    } else {
        g_effect_active_slot_head = g_effect_slots[id].next;
    }
    g_effect_slots[g_effect_slots[id].next].prev = g_effect_slots[id].prev;
    free_head = g_effect_slot_free_head;
    g_effect_slots[id].prev = 0;
    g_effect_slots[id].next = free_head;
    g_effect_slot_free_head = id;
}

/* Start secondary effect animation_id for the caster and first target of the
 * init record and return its id. parameter is the animation's own: an
 * element mask, a sprite palette or a duration. */
s32 effect_init_secondary(s32 animation_id, s32 parameter, effect_secondary_init_t* source) {
    s32 secondary_id;

    secondary_id = effect_allocate_secondary_slot();
    g_effect_secondary_data[secondary_id].animation_id = animation_id;
    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].parameter = parameter;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Start the current effect target's secondary effect: the low half of
 * animation selects the animation id (and its function id), the high half is
 * stored alongside it. Returns its id; phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 effect_init_projectile_secondary(s32 animation, effect_secondary_init_t* source) {
    u8 secondary_id;

    secondary_id = effect_allocate_secondary_slot();
    g_effect_secondary_data[secondary_id].animation_id = animation;
    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].parameter = animation >> 16;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_effect_secondary_data[secondary_id].target_count = source->target_count;
    return secondary_id;
}

/* Returns the effect_element_index_e of an element mask: 0 for no
 * element, otherwise the position of the first table mask it meets (9 when it
 * meets none). */
s32 effect_get_element_index(s32 elements) {
    s32 i;

    if (elements != 0) {
        for (i = 0; i < 9; i++) {
            if ((elements & g_effect_element_masks[i]) != 0) {
                break;
            }
        }
        elements = i;
    }
    return elements;
}

/* Start the current effect target's weapon-hit secondary effect.
 *
 * The target's hit result selects the WEAPON_HIT animation (hit, critical,
 * missed, no effect), EQUIP_BREAK (both BREAK results) or GOLEM; REFLECTED
 * keeps the slot's animation. The parameter
 * is the element effect index for the weapon's element mask. Returns its id,
 * or 0 when the init record has no targets. */
s32 effect_init_weapon_hit_secondary(s32 elements, effect_secondary_init_t* source, s32 unused_is_bow) {
    u8 secondary_id;

    if (source->target_count != 0) {
        elements = effect_get_element_index(elements);
        secondary_id = effect_allocate_secondary_slot();
        switch (source->targets[0].data.unit.result_animation) {
        case EFFECT_HIT_NORMAL:
        case EFFECT_HIT_CRITICAL:
        case EFFECT_HIT_MISSED:
        case EFFECT_HIT_NO_EFFECT:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_WEAPON_HIT;
            break;
        case EFFECT_HIT_BREAK:
        case EFFECT_HIT_BREAK_ALT:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK;
            break;
        case EFFECT_HIT_GOLEM:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_GOLEM;
            break;
        }
        g_effect_secondary_data[secondary_id].parameter = elements;
        g_effect_secondary_data[secondary_id].function_id
            = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
        g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
        g_effect_secondary_data[secondary_id].target.record = source->targets[0];
        g_effect_secondary_data[secondary_id].timer = 0;
        g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    } else {
        secondary_id = 0;
    }
    return secondary_id;
}

/* Start the current effect target's fall-dust secondary effect at the
 * given position and return its id. Phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 effect_init_fall_dust_secondary(effect_secondary_init_t* source, VECTOR* position) {
    s32 secondary_id;

    secondary_id = effect_allocate_secondary_slot();
    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_FALL_DUST;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_FALL_DUST]
              .function_id; /* array access keeps the load after the animation store */
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.position.x = position->vx >> 4;
    g_effect_secondary_data[secondary_id].target.position.z = position->vy >> 4;
    g_effect_secondary_data[secondary_id].target.position.y = position->vz >> 4;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Timer and animation use field-address offsets to retain their store order. */

/* Start EFFECT_SECONDARY_ANIMATION_ENDING in a fresh secondary effect
 * record and return its id. Its null handler only steps through the phases, so
 * the record lasts two more frames. The arrow handler starts it when the arrow
 * lands without a hit. */
s32 effect_end_secondary_on_current_unit(void) {
    s32 secondary_id = effect_allocate_secondary_slot() & 0xFF;
    s32 record_offset = secondary_id * sizeof(effect_secondary_data_t);
    u8 function_id;

    *(s16*)((u8*)&g_effect_secondary_data[0].timer + record_offset) = 0;
    *(u16*)((u8*)&g_effect_secondary_data[0].animation_id + record_offset) = EFFECT_SECONDARY_ANIMATION_ENDING;
    function_id = g_effect_ending_secondary_function_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_effect_secondary_data[secondary_id].function_id = function_id;
    return secondary_id;
}

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
