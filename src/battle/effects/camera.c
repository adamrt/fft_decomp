/* Evaluate effect camera keyframes and start their angle, position and zoom moves. */

#include "fft/camera.h"
#include "fft/battle_map.h"
#include "fft/effect.h"
#include "fft/effect_math.h"
#include "fft/effect_timeline.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Maps a camera quadrant index to its map-tile camera-block direction. */
extern u8 g_camera_quadrant_block_directions[4];

/* Effect-camera track clocks. */
extern s32 g_effect_camera_rotation_duration;
extern s32 g_effect_camera_rotation_frame;

/* reset by camera_angle_command_execute */
extern s32 g_effect_camera_position_duration;
extern s32 g_effect_camera_position_frame;
extern s32 g_effect_camera_zoom_duration;
extern s32 g_effect_camera_zoom_frame;

/* World position of a tile's center (x, layer, y in the SVECTOR): x and z are tile * 28 + 14 and y
 * is the tile surface, -(height * BATTLE_MAP_HEIGHT_STEP), without the depth. */
static void tile_coords_to_world_coords_convert(SVECTOR* in, VECTOR* out) {
    battle_map_tile_t* tile = battle_map_tile_get_pointer(in->vx, in->vz, in->vy);

    out->vx = in->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    out->vz = in->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    out->vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
}

/* Copies the camera rotation with its yaw cut to a multiple of 0x200 (an
 * eighth turn) when the tile's camera-block mask leaves the current yaw
 * quadrant open or blocks every direction; otherwise that yaw turned a quarter
 * turn forward or back, whichever quadrant is open first, else a half turn.
 * Only the half-turn result is wrapped to 0-0xfff. The camera angles are read
 * as an SVECTOR: the quadrant is a signed short division, which unsigned
 * 16-bit values do not emit. */
static void camera_unblocked_rotation_for_tile_choose(const SVECTOR* tile_coords, SVECTOR* rotation_out) {
    battle_map_tile_t* tile = battle_map_tile_get_pointer(tile_coords->vx, tile_coords->vz, tile_coords->vy);
    SVECTOR* rotation;
    s32 mask = tile->camera_block_masks & BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    s32 quadrant;
    s32 yaw;

    rotation = camera_rotation_get();
    rotation_out->vx = rotation->vx;
    rotation_out->vz = rotation->vz;
    quadrant = (rotation->vy & 0xfff) / 0x400;
    yaw = rotation->vy & 0xe00;
    if ((g_camera_quadrant_block_directions[quadrant] & mask) == 0
        || mask == BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK) {
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
void effect_camera_noop_1(s32 ignored_1, s32 ignored_2) {
    s32* unused = &ignored_1;
    unused = &ignored_2;
}

/* Writes the map center as the effect camera target: x = max x * 14 and
 * z = max y * 14 (half of a 28-unit tile per tile), with the height (vy) 0. */
static void camera_map_center_coordinates_store(VECTOR* destination) {
    SVECTOR map_max;
    s32 max_y;

    battle_map_dimensions_store(&map_max);
    destination->vx = map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
    max_y = map_max.vz;
    destination->vy = 0;
    destination->vz = max_y * (BATTLE_MAP_TILE_SIZE / 2);
}

/* Timeline track 1 handler: sets the effect camera angle target and duration.
 *
 * Mode bits 0x1e0 pick the source: the best unblocked view of a target's or
 * the caster's tile (0x0, 0x140) or of the cursor tile (0x1c0), the given
 * angles with the yaw wrapped to the nearest turn (0x40), the current
 * rotation (0xc0), the given pitch with offsets added to current yaw/roll (0x20), the
 * saved pitch with the current yaw/roll (0x60) or the saved angles with the yaw
 * wrapped against g_effect_camera_rotation_current (0x100). Most modes then add `angles` when
 * present. Bits 0x1e00 are kept in g_effect_camera_rotation_mode; other modes return untouched.
 *
 * Mode 0x40 keeps its own `current` pointer: sharing `rotation` with the other
 * modes changes that case's register allocation. */
static void camera_angle_command_execute(s32 command, s32 target, s32 duration, const SVECTOR* angles) {
    SVECTOR coords;
    SVECTOR* rotation;
    SVECTOR* current;
    s16 delta;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            unit_runtime_map_coords_get(g_effect_context.targets[target].data.unit.runtime_id, &coords);
        } else {
            coords = g_effect_context.targets[target].data.tile;
        }
        camera_unblocked_rotation_for_tile_choose(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        current = camera_rotation_get();
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
        rotation = camera_rotation_get();
        copyVector(&g_effect_camera_rotation_target, rotation);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_rotation_target = g_effect_camera_rotation_saved;
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
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
            unit_runtime_map_coords_get(g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id, &coords);
        } else {
            coords = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile;
        }
        camera_unblocked_rotation_for_tile_choose(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        coords = g_effect_context.target_tile;
        camera_unblocked_rotation_for_tile_choose(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT_YAW_ROLL:
        rotation = camera_rotation_get();
        if (angles) {
            setVector(
                &g_effect_camera_rotation_target, angles->vx, rotation->vy + angles->vy, rotation->vz + angles->vz);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED_PITCH:
        rotation = camera_rotation_get();
        setVector(&g_effect_camera_rotation_target, g_effect_camera_rotation_saved.vx, rotation->vy, rotation->vz);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
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
 * targets (0x180) or the tile at g_effect_context.target_tile (0x1c0). All but 0x40 add offset
 * when present. Bits 0x1e00 are kept in g_effect_camera_position_mode. */
void effect_camera_execute_position_command(s32 command, s32 target, s32 duration, const VECTOR* offset) {
    VECTOR sum;
    VECTOR* coords;
    SVECTOR* world;
    s32 i;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_copy_unit_world_coords_to_vector(
                g_effect_context.targets[target].data.unit.runtime_id, &g_effect_camera_position_target);
        } else {
            tile_coords_to_world_coords_convert(
                &g_effect_context.targets[target].data.tile, &g_effect_camera_position_target);
        }
        coords = camera_real_coords_get();
        effect_vector_shift_right_12(coords, &g_effect_camera_position_start);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        g_effect_camera_position_target = *offset;
        break;
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
        camera_map_center_coordinates_store(&g_effect_camera_position_target);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        coords = camera_real_coords_get();
        setVector(&g_effect_camera_position_target, coords->vx >> 12, coords->vy >> 12, coords->vz >> 12);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_position_target = g_effect_camera_position_saved;
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CASTER:
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_copy_unit_world_coords_to_vector(
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id, &g_effect_camera_position_target);
        } else {
            tile_coords_to_world_coords_convert(
                &g_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile, &g_effect_camera_position_target);
        }
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        /* Retail divides by every target, units or not, and stores the average only
         * when an offset is given; without one the target position is left as it was. */
        setVector(&sum, 0, 0, 0);
        for (i = 0; i < g_effect_context.target_count; i++) {
            if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                world = unit_runtime_world_coords_get(g_effect_context.targets[i].data.unit.runtime_id);
                sum.vx += world->vx;
                sum.vy += world->vy;
                sum.vz += world->vz;
            }
        }
        sum.vx /= i;
        sum.vy /= i;
        sum.vz /= i;
        if (offset) {
            effect_vector_add(offset, &sum, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        tile_coords_to_world_coords_convert(&g_effect_context.target_tile, &g_effect_camera_position_target);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
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
void effect_camera_execute_zoom_command(s32 command, s32 unused, s32 duration, const VECTOR* zoom) {
    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
    case EFFECT_CAMERA_SOURCE_GIVEN:
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
    case EFFECT_CAMERA_SOURCE_CASTER:
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        g_effect_camera_zoom_target = *zoom;
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        g_effect_camera_zoom_target = *camera_zoom_get();
        if (zoom) {
            effect_vector_add(zoom, &g_effect_camera_zoom_target, &g_effect_camera_zoom_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_zoom_target = g_effect_camera_zoom_saved;
        if (zoom) {
            effect_vector_add(zoom, &g_effect_camera_zoom_target, &g_effect_camera_zoom_target);
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
void effect_camera_update_tracks(void) {
    VECTOR position;
    VECTOR delta;
    SVECTOR rotation;
    SVECTOR rotation_delta;

    switch (g_effect_camera_rotation_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        camera_rotation_set(&g_effect_camera_rotation_target);
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
            effect_svector_cos_interpolate(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            effect_vector_mask_low_12_bits(&g_effect_camera_rotation_target, &g_effect_camera_rotation_start);
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            camera_rotation_set(&g_effect_camera_rotation_start);
        }
        break;
    /* An out-and-back move ends where it began, so the start stays as it is. */
    case EFFECT_CAMERA_MODE_COSINE_OUT_AND_BACK:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_svector_cos_out_back_interpolate(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_svector_linear_interpolate(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            effect_vector_mask_low_12_bits(&g_effect_camera_rotation_target, &g_effect_camera_rotation_start);
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            camera_rotation_set(&g_effect_camera_rotation_start);
        }
        break;
    /* The ADD modes treat the target as a per-frame step: ramped up from zero, constant, or
     * ramped down to zero. At the end the accumulated angle becomes both start and target. */
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_svector_linear_interpolate(&rotation, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_rotations_mod_q12(
                &g_effect_camera_rotation_current, &rotation_delta, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
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
            effect_vector_add_rotations_mod_q12(
                &g_effect_camera_rotation_current, &g_effect_camera_rotation_target, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
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
            effect_svector_linear_interpolate(&g_effect_camera_rotation_target, &rotation,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_rotations_mod_q12(
                &g_effect_camera_rotation_current, &rotation_delta, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_target = g_effect_camera_rotation_current;
            g_effect_camera_rotation_start = g_effect_camera_rotation_target;
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    /* The SHAKE modes treat the target as a random range around the start angle. */
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_svector_linear_interpolate(&rotation, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_random_rotation_offsets(&g_effect_camera_rotation_start, &rotation_delta, &rotation);
            camera_rotation_set(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_vector_add_random_rotation_offsets(
                &g_effect_camera_rotation_start, &g_effect_camera_rotation_target, &rotation);
            camera_rotation_set(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_svector_linear_interpolate(&g_effect_camera_rotation_target, &rotation,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_random_rotation_offsets(&g_effect_camera_rotation_start, &rotation_delta, &rotation);
            camera_rotation_set(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    /* Position track: the same modes on whole-unit values, scaled by ONE for the camera. */
    switch (g_effect_camera_position_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        effect_vector_scale_q12(&g_effect_camera_position_target, &position);
        camera_real_coords_set(&position);
        g_effect_camera_position_current = g_effect_camera_position_target;
        g_effect_camera_position_start = g_effect_camera_position_current;
        g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_position_frame = g_effect_camera_position_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_vector_q13_cos_interpolate(&g_effect_camera_position_start, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &g_effect_camera_position_current);
            effect_vector_scale_q12(&g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_OUT_AND_BACK:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_vector_q13_cos_out_back_interpolate(&g_effect_camera_position_start,
                &g_effect_camera_position_target, g_effect_camera_position_duration, g_effect_camera_position_frame,
                &g_effect_camera_position_current);
            effect_vector_scale_q12(&g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_vector_q13_linear_interpolate(&g_effect_camera_position_start, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &g_effect_camera_position_current);
            effect_vector_scale_q12(&g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
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
            effect_vector_q13_linear_interpolate(&position, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_and_store_q12(&delta, &g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
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
            effect_vector_add_and_store_q12(
                &g_effect_camera_position_target, &g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
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
            effect_vector_q13_linear_interpolate(&g_effect_camera_position_target, &position,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_and_store_q12(&delta, &g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
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
            effect_vector_q13_linear_interpolate(&position, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_random_offsets_and_store_q12(&g_effect_camera_position_start, &delta, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_vector_add_random_offsets_and_store_q12(
                &g_effect_camera_position_start, &g_effect_camera_position_target, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_vector_q13_linear_interpolate(&g_effect_camera_position_target, &position,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_random_offsets_and_store_q12(&g_effect_camera_position_start, &delta, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    /* Zoom track: the same modes, with no angle wrap or scaling. */
    switch (g_effect_camera_zoom_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        camera_zoom_set(&g_effect_camera_zoom_target);
        g_effect_camera_zoom_current = g_effect_camera_zoom_target;
        g_effect_camera_zoom_start = g_effect_camera_zoom_current;
        g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_zoom_frame = g_effect_camera_zoom_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_vector_q13_cos_interpolate(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_OUT_AND_BACK:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_vector_q13_cos_out_back_interpolate(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_vector_q13_linear_interpolate(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
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
            effect_vector_q13_linear_interpolate(&position, &g_effect_camera_zoom_target, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add(&delta, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
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
            effect_vector_add(
                &g_effect_camera_zoom_target, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
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
            effect_vector_q13_linear_interpolate(&g_effect_camera_zoom_target, &position, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add(&delta, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
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
            effect_vector_q13_linear_interpolate(&position, &g_effect_camera_zoom_target, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add_random_offsets(&g_effect_camera_zoom_start, &delta, &position);
            camera_zoom_set(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_vector_add_random_offsets(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target, &position);
            camera_zoom_set(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_vector_q13_linear_interpolate(&g_effect_camera_zoom_target, &position, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add_random_offsets(&g_effect_camera_zoom_start, &delta, &position);
            camera_zoom_set(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }
}

/* Find the first child-table camera keyframe whose command moves a track in mask and whose end
 * frame is still ahead of frame; report its index and end frame and return 1. Returns 0 at or past
 * the child duration, or when none is left. The single-table form of
 * camera_active_keyframe_find, with no target in the end frame's high half. */
static s32 camera_next_keyframe_find(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    effect_timeline_t* data = g_effect_timeline;
    s32 i;

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
static s32 camera_active_keyframe_find(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    s32 phase1 = g_effect_timeline->phase1_duration;
    s32 spawn_delay;
    s32 child_duration;
    s32 i;
    s32 target_index;
    s32 local_frame;
    s32 elapsed;
    s32 start;

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
void effect_camera_start_child_keyframe_moves(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_next_keyframe_find(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        effect_vector_load_svector(&g_effect_timeline->rotation[index], &rotation);
        camera_angle_command_execute(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &rotation);
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_next_keyframe_find(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        effect_vector_load(&g_effect_timeline->position[index], &vector);
        effect_camera_execute_position_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_next_keyframe_find(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        effect_vector_load(&g_effect_timeline->zoom[index], &vector);
        effect_camera_execute_zoom_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
}

/* Starts the parent timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the table camera_active_keyframe_find reports (phase 1, child
 * or phase 2). The three-table form of
 * effect_camera_start_child_keyframe_moves. */
void effect_camera_start_parent_keyframe_moves(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;
    s32 k;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_active_keyframe_find(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_vector_load_svector(&g_effect_timeline->phase1_camera.rotation[k], &rotation);
            camera_angle_command_execute(g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_vector_load_svector(&g_effect_timeline->rotation[k], &rotation);
            camera_angle_command_execute(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_vector_load_svector(&g_effect_timeline->phase2_camera.rotation[k], &rotation);
            camera_angle_command_execute(g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        }
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_active_keyframe_find(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_vector_load(&g_effect_timeline->phase1_camera.position[k], &vector);
            effect_camera_execute_position_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_vector_load(&g_effect_timeline->position[k], &vector);
            effect_camera_execute_position_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_vector_load(&g_effect_timeline->phase2_camera.position[k], &vector);
            effect_camera_execute_position_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_active_keyframe_find(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_vector_load(&g_effect_timeline->phase1_camera.zoom[k], &vector);
            effect_camera_execute_zoom_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_vector_load(&g_effect_timeline->zoom[k], &vector);
            effect_camera_execute_zoom_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_vector_load(&g_effect_timeline->phase2_camera.zoom[k], &vector);
            effect_camera_execute_zoom_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
}
