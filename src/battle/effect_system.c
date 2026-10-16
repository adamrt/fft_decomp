/* The effect system, including camera keyframes, effect slots, projectile trajectories, secondary effects and their
 * handlers. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* World position of a tile's center (x, layer, y in the SVECTOR): x and z are tile * 28 + 14 and y
 * is the tile surface, -(height * 12), without the depth. */
static void battle_effect_convert_tile_coords_to_world_coords(SVECTOR* in, VECTOR* out) {
    map_tile_t* tile = battle_map_get_tile_data_pointer(in->vx, in->vz, in->vy);

    out->vx = in->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    out->vz = in->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    out->vy = -(tile->height * 12);
}

/* Copies the camera rotation with its yaw cut to a multiple of 0x200 (an
 * eighth turn) when the tile's camera-block mask leaves the current yaw
 * quadrant open or blocks every direction; otherwise that yaw turned a quarter
 * turn forward or back, whichever quadrant is open first, else a half turn.
 * Only the half-turn result is wrapped to 0-0xfff. The camera angles are read
 * as an SVECTOR: the quadrant is a signed short division, which unsigned
 * 16-bit values do not emit. */
static void battle_camera_choose_unblocked_rotation_for_tile(const SVECTOR* tile_coords, SVECTOR* rotation_out) {
    map_tile_t* tile = battle_map_get_tile_data_pointer(tile_coords->vx, tile_coords->vz, tile_coords->vy);
    SVECTOR* rotation;
    s32 mask = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    s32 quadrant;
    s32 yaw;

    rotation = battle_camera_get_rotation();
    rotation_out->vx = rotation->vx;
    rotation_out->vz = rotation->vz;
    quadrant = (rotation->vy & 0xfff) / 0x400;
    yaw = rotation->vy & 0xe00;
    if ((g_battle_camera_quadrant_block_directions[quadrant] & mask) == 0
        || mask == MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK) {
        rotation_out->vy = yaw;
        return;
    }
    if ((g_battle_camera_quadrant_block_directions[(quadrant + 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw + 0x400;
    } else if ((g_battle_camera_quadrant_block_directions[(quadrant - 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw - 0x400;
    } else {
        rotation_out->vy = (yaw + 0x800) & 0xfff;
    }
}

/* The 12-byte target stores both arguments to their incoming stack home slots
 * and returns. Taking their addresses reproduces those stores; it does not
 * establish what the original source body contained. */
void battle_noop_801aad30(s32 ignored_1, s32 ignored_2) {
    s32* unused = &ignored_1;
    unused = &ignored_2;
}

/* Writes the map center as the effect camera target: x = max x * 14 and
 * z = max y * 14 (half of a 28-unit tile per tile), with the height (vy) 0. */
static void battle_effect_store_map_center_coordinates(VECTOR* destination) {
    SVECTOR map_max;
    s32 max_y;

    battle_map_store_max_coordinates(&map_max);
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
 * wrapped against g_battle_effect_camera_rotation_current (0x100). Most modes then add `angles` when
 * present. Bits 0x1e00 are kept in g_battle_effect_camera_rotation_mode; other modes return untouched.
 *
 * Mode 0x40 keeps its own `current` pointer: sharing `rotation` with the other
 * modes changes that case's register allocation. */
static void battle_camera_execute_angle_command(s32 command, s32 target, s32 duration, const SVECTOR* angles) {
    SVECTOR coords;
    SVECTOR* rotation;
    SVECTOR* current;
    s16 delta;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_battle_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            battle_unit_get_map_coords_by_runtime_id(
                g_battle_effect_context.targets[target].data.unit.runtime_id, &coords);
        } else {
            coords = g_battle_effect_context.targets[target].data.tile;
        }
        battle_camera_choose_unblocked_rotation_for_tile(&coords, &g_battle_effect_camera_rotation_target);
        if (angles) {
            battle_effect_add_svectors(
                angles, &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        current = battle_camera_get_rotation();
        delta = angles->vy - current->vy;
        if (delta < -0x800) {
            delta += ONE;
        } else if (delta > 0x800) {
            delta -= ONE;
        }
        setVector(&g_battle_effect_camera_rotation_target, angles->vx, delta + current->vy, angles->vz);
        g_battle_effect_camera_rotation_mode = command & EFFECT_CAMERA_MODE_MASK;
        g_battle_effect_camera_rotation_duration = duration;
        g_battle_effect_camera_rotation_frame = 0;
        return;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        rotation = battle_camera_get_rotation();
        copyVector(&g_battle_effect_camera_rotation_target, rotation);
        if (angles) {
            battle_effect_add_svectors(
                angles, &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_battle_effect_camera_rotation_target = g_battle_effect_camera_rotation_saved;
        if (angles) {
            battle_effect_add_svectors(
                angles, &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_target);
        }
        delta = g_battle_effect_camera_rotation_target.vy - g_battle_effect_camera_rotation_current.vy;
        if (delta > 0x800) {
            g_battle_effect_camera_rotation_target.vy -= ONE;
        }
        if (delta < -0x800) {
            g_battle_effect_camera_rotation_target.vy += ONE;
        }
        break;
    case EFFECT_CAMERA_SOURCE_CASTER:
        if (g_battle_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            battle_unit_get_map_coords_by_runtime_id(
                g_battle_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id, &coords);
        } else {
            coords = g_battle_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile;
        }
        battle_camera_choose_unblocked_rotation_for_tile(&coords, &g_battle_effect_camera_rotation_target);
        if (angles) {
            battle_effect_add_svectors(
                angles, &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        coords = g_battle_effect_target_tile.data.tile;
        battle_camera_choose_unblocked_rotation_for_tile(&coords, &g_battle_effect_camera_rotation_target);
        if (angles) {
            battle_effect_add_svectors(
                angles, &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT_YAW_ROLL:
        rotation = battle_camera_get_rotation();
        if (angles) {
            setVector(&g_battle_effect_camera_rotation_target, angles->vx, rotation->vy + angles->vy,
                rotation->vz + angles->vz);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED_PITCH:
        rotation = battle_camera_get_rotation();
        setVector(&g_battle_effect_camera_rotation_target, g_battle_effect_camera_rotation_saved.vx, rotation->vy,
            rotation->vz);
        if (angles) {
            battle_effect_add_svectors(
                angles, &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_target);
        }
        break;
    default:
        return;
    }
    g_battle_effect_camera_rotation_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_battle_effect_camera_rotation_duration = duration;
    g_battle_effect_camera_rotation_frame = 0;
}

/* Timeline track 2 handler: sets the effect camera position target and duration.
 *
 * Mode bits 0x1e0 pick the source: effect target `target` (0x0, which also
 * saves the current camera position to g_battle_effect_camera_position_start), the given vector (0x40),
 * the map center (0x80), the current camera (0xc0), the saved position at
 * g_battle_effect_camera_position_saved (0x100), the caster entry (0x140), the average of all unit
 * targets (0x180) or the tile at g_battle_effect_target_tile (0x1c0). All but 0x40 add offset
 * when present. Bits 0x1e00 are kept in g_battle_effect_camera_position_mode. */
static void battle_camera_execute_position_command(s32 command, s32 target, s32 duration, const VECTOR* offset) {
    VECTOR sum;
    VECTOR* coords;
    SVECTOR* screen;
    s32 i;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_battle_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            battle_effect_copy_unit_world_coords_to_vector(
                g_battle_effect_context.targets[target].data.unit.runtime_id, &g_battle_effect_camera_position_target);
        } else {
            battle_effect_convert_tile_coords_to_world_coords(
                &g_battle_effect_context.targets[target].data.tile, &g_battle_effect_camera_position_target);
        }
        coords = battle_camera_get_current_real_coords();
        battle_effect_shift_vector_right_12(coords, &g_battle_effect_camera_position_start);
        if (offset) {
            battle_effect_add_vectors(
                offset, &g_battle_effect_camera_position_target, &g_battle_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        g_battle_effect_camera_position_target = *offset;
        break;
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
        battle_effect_store_map_center_coordinates(&g_battle_effect_camera_position_target);
        if (offset) {
            battle_effect_add_vectors(
                offset, &g_battle_effect_camera_position_target, &g_battle_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        coords = battle_camera_get_current_real_coords();
        setVector(&g_battle_effect_camera_position_target, coords->vx >> 12, coords->vy >> 12, coords->vz >> 12);
        if (offset) {
            battle_effect_add_vectors(
                offset, &g_battle_effect_camera_position_target, &g_battle_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_battle_effect_camera_position_target = g_battle_effect_camera_position_saved;
        if (offset) {
            battle_effect_add_vectors(
                offset, &g_battle_effect_camera_position_target, &g_battle_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CASTER:
        if (g_battle_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            battle_effect_copy_unit_world_coords_to_vector(
                g_battle_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id,
                &g_battle_effect_camera_position_target);
        } else {
            battle_effect_convert_tile_coords_to_world_coords(
                &g_battle_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile,
                &g_battle_effect_camera_position_target);
        }
        if (offset) {
            battle_effect_add_vectors(
                offset, &g_battle_effect_camera_position_target, &g_battle_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        setVector(&sum, 0, 0, 0);
        for (i = 0; i < g_battle_effect_context.target_count; i++) {
            if (g_battle_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                screen = battle_unit_get_world_coords_by_runtime_id(
                    g_battle_effect_context.targets[i].data.unit.runtime_id);
                sum.vx += screen->vx;
                sum.vy += screen->vy;
                sum.vz += screen->vz;
            }
        }
        sum.vx /= i;
        sum.vy /= i;
        sum.vz /= i;
        if (offset) {
            battle_effect_add_vectors(offset, &sum, &g_battle_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        battle_effect_convert_tile_coords_to_world_coords(
            &g_battle_effect_target_tile.data.tile, &g_battle_effect_camera_position_target);
        if (offset) {
            battle_effect_add_vectors(
                offset, &g_battle_effect_camera_position_target, &g_battle_effect_camera_position_target);
        }
        break;
    default:
        return;
    }
    g_battle_effect_camera_position_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_battle_effect_camera_position_duration = duration;
    g_battle_effect_camera_position_frame = 0;
}

/* Timeline track 3 handler: sets the effect camera zoom target and duration.
 *
 * Mode bits 0x1e0 pick the source: the given vector, the current camera zoom
 * (0xc0) or the saved zoom at g_battle_effect_camera_zoom_saved (0x100), the latter two offset by
 * zoom when present. Other modes leave the camera untouched. Bits 0x1e00 are
 * kept in g_battle_effect_camera_zoom_mode. */
static void battle_camera_execute_zoom_command(s32 command, s32 unused, s32 duration, const VECTOR* zoom) {
    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
    case EFFECT_CAMERA_SOURCE_GIVEN:
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
    case EFFECT_CAMERA_SOURCE_CASTER:
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        g_battle_effect_camera_zoom_target = *zoom;
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        g_battle_effect_camera_zoom_target = *battle_camera_get_zoom();
        if (zoom) {
            battle_effect_add_vectors(zoom, &g_battle_effect_camera_zoom_target, &g_battle_effect_camera_zoom_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_saved;
        if (zoom) {
            battle_effect_add_vectors(zoom, &g_battle_effect_camera_zoom_target, &g_battle_effect_camera_zoom_target);
        }
        break;
    default:
        return;
    }
    g_battle_effect_camera_zoom_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_battle_effect_camera_zoom_duration = duration;
    g_battle_effect_camera_zoom_frame = 0;
}

/* Advance the effect camera rotation, position and zoom tracks one frame.
 *
 * Each track's 32-bit mode (g_battle_effect_camera_rotation_mode, _position_mode and _zoom_mode: bits 0x1e00 of the
 * timeline command) selects an interpolation between the start and target values over the track duration; the mode
 * clears once the counter reaches the duration. */
void battle_camera_update_effect_rotation_and_zoom(void) {
    VECTOR position;
    VECTOR delta;
    SVECTOR rotation;
    SVECTOR rotation_delta;

    switch (g_battle_effect_camera_rotation_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        battle_camera_set_rotation(&g_battle_effect_camera_rotation_target);
        g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        g_battle_effect_camera_rotation_start.vx = g_battle_effect_camera_rotation_current.vx
            = g_battle_effect_camera_rotation_target.vx & 0xfff;
        g_battle_effect_camera_rotation_start.vy = g_battle_effect_camera_rotation_current.vy
            = g_battle_effect_camera_rotation_target.vy & 0xfff;
        g_battle_effect_camera_rotation_start.vz = g_battle_effect_camera_rotation_current.vz
            = g_battle_effect_camera_rotation_target.vz & 0xfff;
        g_battle_effect_camera_rotation_frame = g_battle_effect_camera_rotation_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_interpolate_svector_cos(&g_battle_effect_camera_rotation_start,
                &g_battle_effect_camera_rotation_target, g_battle_effect_camera_rotation_duration,
                g_battle_effect_camera_rotation_frame, &g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            battle_effect_mask_vector_low_12_bits(
                &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_start);
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_start);
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_Q12:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_interpolate_svector_cos_q12(&g_battle_effect_camera_rotation_start,
                &g_battle_effect_camera_rotation_target, g_battle_effect_camera_rotation_duration,
                g_battle_effect_camera_rotation_frame, &g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_interpolate_svector_linear(&g_battle_effect_camera_rotation_start,
                &g_battle_effect_camera_rotation_target, g_battle_effect_camera_rotation_duration,
                g_battle_effect_camera_rotation_frame, &g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            battle_effect_mask_vector_low_12_bits(
                &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_start);
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_start);
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&rotation, &g_battle_effect_camera_rotation_target,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta);
            battle_effect_add_rotation_vectors_mod_q12(
                &g_battle_effect_camera_rotation_current, &rotation_delta, &g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_target = g_battle_effect_camera_rotation_current;
            g_battle_effect_camera_rotation_start = g_battle_effect_camera_rotation_target;
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_add_rotation_vectors_mod_q12(&g_battle_effect_camera_rotation_current,
                &g_battle_effect_camera_rotation_target, &g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_target = g_battle_effect_camera_rotation_current;
            g_battle_effect_camera_rotation_start = g_battle_effect_camera_rotation_target;
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&g_battle_effect_camera_rotation_target, &rotation,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta);
            battle_effect_add_rotation_vectors_mod_q12(
                &g_battle_effect_camera_rotation_current, &rotation_delta, &g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation(&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_target = g_battle_effect_camera_rotation_current;
            g_battle_effect_camera_rotation_start = g_battle_effect_camera_rotation_target;
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&rotation, &g_battle_effect_camera_rotation_target,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta);
            battle_effect_add_random_rotation_offsets(
                &g_battle_effect_camera_rotation_start, &rotation_delta, &rotation);
            battle_camera_set_rotation(&rotation);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_add_random_rotation_offsets(
                &g_battle_effect_camera_rotation_start, &g_battle_effect_camera_rotation_target, &rotation);
            battle_camera_set_rotation(&rotation);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&g_battle_effect_camera_rotation_target, &rotation,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta);
            battle_effect_add_random_rotation_offsets(
                &g_battle_effect_camera_rotation_start, &rotation_delta, &rotation);
            battle_camera_set_rotation(&rotation);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    switch (g_battle_effect_camera_position_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        battle_effect_scale_vector_fixed12(&g_battle_effect_camera_position_target, &position);
        battle_camera_set_current_real_coords(&position);
        g_battle_effect_camera_position_current = g_battle_effect_camera_position_target;
        g_battle_effect_camera_position_start = g_battle_effect_camera_position_current;
        g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        g_battle_effect_camera_position_frame = g_battle_effect_camera_position_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_interpolate_vector_q13_cos(&g_battle_effect_camera_position_start,
                &g_battle_effect_camera_position_target, g_battle_effect_camera_position_duration,
                g_battle_effect_camera_position_frame, &g_battle_effect_camera_position_current);
            battle_effect_scale_vector_fixed12(&g_battle_effect_camera_position_current, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_Q12:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_interpolate_vector_q13_cos_q12(&g_battle_effect_camera_position_start,
                &g_battle_effect_camera_position_target, g_battle_effect_camera_position_duration,
                g_battle_effect_camera_position_frame, &g_battle_effect_camera_position_current);
            battle_effect_scale_vector_fixed12(&g_battle_effect_camera_position_current, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_position_start,
                &g_battle_effect_camera_position_target, g_battle_effect_camera_position_duration,
                g_battle_effect_camera_position_frame, &g_battle_effect_camera_position_current);
            battle_effect_scale_vector_fixed12(&g_battle_effect_camera_position_current, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position, &g_battle_effect_camera_position_target,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta);
            battle_effect_add_vector_and_store_q12(&delta, &g_battle_effect_camera_position_current, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_target = g_battle_effect_camera_position_current;
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_add_vector_and_store_q12(
                &g_battle_effect_camera_position_target, &g_battle_effect_camera_position_current, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_target = g_battle_effect_camera_position_current;
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_position_target, &position,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta);
            battle_effect_add_vector_and_store_q12(&delta, &g_battle_effect_camera_position_current, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_target = g_battle_effect_camera_position_current;
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position, &g_battle_effect_camera_position_target,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta);
            battle_effect_add_random_vector_offsets_and_store_q12(
                &g_battle_effect_camera_position_start, &delta, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_add_random_vector_offsets_and_store_q12(
                &g_battle_effect_camera_position_start, &g_battle_effect_camera_position_target, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_position_target, &position,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta);
            battle_effect_add_random_vector_offsets_and_store_q12(
                &g_battle_effect_camera_position_start, &delta, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    switch (g_battle_effect_camera_zoom_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        battle_camera_set_zoom(&g_battle_effect_camera_zoom_target);
        g_battle_effect_camera_zoom_current = g_battle_effect_camera_zoom_target;
        g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_current;
        g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        g_battle_effect_camera_zoom_frame = g_battle_effect_camera_zoom_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_interpolate_vector_q13_cos(&g_battle_effect_camera_zoom_start,
                &g_battle_effect_camera_zoom_target, g_battle_effect_camera_zoom_duration,
                g_battle_effect_camera_zoom_frame, &g_battle_effect_camera_zoom_current);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_Q12:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_interpolate_vector_q13_cos_q12(&g_battle_effect_camera_zoom_start,
                &g_battle_effect_camera_zoom_target, g_battle_effect_camera_zoom_duration,
                g_battle_effect_camera_zoom_frame, &g_battle_effect_camera_zoom_current);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_zoom_start,
                &g_battle_effect_camera_zoom_target, g_battle_effect_camera_zoom_duration,
                g_battle_effect_camera_zoom_frame, &g_battle_effect_camera_zoom_current);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position, &g_battle_effect_camera_zoom_target,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta);
            battle_effect_add_vectors(
                &delta, &g_battle_effect_camera_zoom_current, &g_battle_effect_camera_zoom_current);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_current;
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_add_vectors(&g_battle_effect_camera_zoom_target, &g_battle_effect_camera_zoom_current,
                &g_battle_effect_camera_zoom_current);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_current;
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_zoom_target, &position,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta);
            battle_effect_add_vectors(
                &delta, &g_battle_effect_camera_zoom_current, &g_battle_effect_camera_zoom_current);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_current;
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position, &g_battle_effect_camera_zoom_target,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta);
            battle_effect_add_random_vector_offsets(&g_battle_effect_camera_zoom_start, &delta, &position);
            battle_camera_set_zoom(&position);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_add_random_vector_offsets(
                &g_battle_effect_camera_zoom_start, &g_battle_effect_camera_zoom_target, &position);
            battle_camera_set_zoom(&position);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_zoom_target, &position,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta);
            battle_effect_add_random_vector_offsets(&g_battle_effect_camera_zoom_start, &delta, &position);
            battle_camera_set_zoom(&position);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }
}

/* Find the first child-table camera keyframe whose command moves a track in mask and whose end
 * frame is still ahead of frame; report its index and end frame and return 1. Returns 0 at or past
 * the child duration, or when none is left. The single-table form of
 * battle_camera_find_active_keyframe, with no target in the end frame's high half. */
static s32 battle_effect_find_next_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    effect_timeline_t* data = g_battle_effect_timeline;
    s32 i;

    if (frame < data->child.duration) {
        i = 0;
        if (data->last_keyframe_index >= 0) {
            do {
                if (frame < data->end_frame[i] && (mask & data->command[i]) != 0) {
                    *out_index = i;
                    *out_frame = g_battle_effect_timeline->end_frame[i];
                    return 1;
                }
                i++;
                data = g_battle_effect_timeline;
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
static s32 battle_camera_find_active_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    s32 phase1 = g_battle_effect_timeline->phase1_duration;
    s32 spawn_delay;
    s32 child_duration;
    s32 i;
    s32 target_index;
    s32 local_frame;
    s32 elapsed;
    s32 start;

    spawn_delay = g_battle_effect_context.target_count != 0 ? g_battle_effect_timeline->spawn_delay : 0;
    child_duration = g_battle_effect_context.target_count != 0 ? g_battle_effect_timeline->child.duration : 0;

    if (frame < phase1) {
        for (i = 0; i <= g_battle_effect_timeline->phase1_camera.last_keyframe_index; i++) {
            if (frame < g_battle_effect_timeline->phase1_camera.end_frame[i]
                && (mask & g_battle_effect_timeline->phase1_camera.command[i]) != 0) {
                *out_index = i;
                *out_frame = g_battle_effect_timeline->phase1_camera.end_frame[i];
                return 1;
            }
        }
    }

    if (spawn_delay != 0 && frame < phase1 + (g_battle_effect_context.target_count - 1) * spawn_delay) {
        target_index = 0;
        if (frame >= phase1) {
            elapsed = frame - phase1;
            target_index = elapsed / spawn_delay;
            local_frame = elapsed - target_index * spawn_delay;
        } else {
            local_frame = -1;
        }
        for (i = 0; i <= g_battle_effect_timeline->last_keyframe_index; i++) {
            if (g_battle_effect_timeline->end_frame[i] >= spawn_delay) {
                break;
            }
            if (local_frame < g_battle_effect_timeline->end_frame[i]
                && (mask & g_battle_effect_timeline->command[i]) != 0) {
                *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                *out_frame = (target_index << 16)
                    + (g_battle_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                return 1;
            }
        }
        target_index++;
        local_frame = -1;
        if (target_index < g_battle_effect_context.target_count) {
            for (i = 0; i <= g_battle_effect_timeline->last_keyframe_index; i++) {
                if (g_battle_effect_timeline->end_frame[i] >= spawn_delay) {
                    break;
                }
                if (local_frame < g_battle_effect_timeline->end_frame[i]
                    && (mask & g_battle_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_battle_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (g_battle_effect_context.target_count != 0) {
        target_index = g_battle_effect_context.target_count - 1;
        start = phase1 + target_index * spawn_delay;
        if (frame < start + child_duration) {
            local_frame = -1;
            if (frame >= start) {
                local_frame = frame - phase1 - target_index * spawn_delay;
            }
            for (i = 0; i <= g_battle_effect_timeline->last_keyframe_index; i++) {
                if (local_frame < g_battle_effect_timeline->end_frame[i]
                    && (mask & g_battle_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_battle_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (frame >= phase1 + (g_battle_effect_context.target_count - 1) * spawn_delay + child_duration) {
        local_frame = frame - phase1 - (g_battle_effect_context.target_count - 1) * spawn_delay - child_duration;
    } else {
        local_frame = -1;
    }
    for (i = 0; i <= g_battle_effect_timeline->phase2_camera.last_keyframe_index; i++) {
        if (local_frame < g_battle_effect_timeline->phase2_camera.end_frame[i]
            && (mask & g_battle_effect_timeline->phase2_camera.command[i]) != 0) {
            *out_index = i + (EFFECT_CAMERA_TABLE_PHASE2 << 16);
            *out_frame = g_battle_effect_timeline->phase2_camera.end_frame[i] + phase1
                + (g_battle_effect_context.target_count - 1) * spawn_delay + child_duration;
            return 1;
        }
    }
    return 0;
}

/* Starts the child timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the child camera table. */
void battle_camera_start_effect_keyframe_moves(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;

    if (g_battle_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && battle_effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        battle_effect_load_svector(&g_battle_effect_timeline->rotation[index], &rotation);
        battle_camera_execute_angle_command(
            g_battle_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &rotation);
    }
    if (g_battle_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && battle_effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        battle_effect_load_vector(&g_battle_effect_timeline->position[index], &vector);
        battle_camera_execute_position_command(
            g_battle_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
    if (g_battle_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && battle_effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        battle_effect_load_vector(&g_battle_effect_timeline->zoom[index], &vector);
        battle_camera_execute_zoom_command(
            g_battle_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
}

/* Starts the parent timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the table battle_camera_find_active_keyframe reports (phase 1, child
 * or phase 2). The three-table form of
 * battle_camera_start_effect_keyframe_moves. */
void battle_camera_advance_effect_tracks(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;
    s32 k;

    if (g_battle_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && battle_camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            battle_effect_load_svector(&g_battle_effect_timeline->phase1_camera.rotation[k], &rotation);
            battle_camera_execute_angle_command(g_battle_effect_timeline->phase1_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            battle_effect_load_svector(&g_battle_effect_timeline->rotation[k], &rotation);
            battle_camera_execute_angle_command(
                g_battle_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            battle_effect_load_svector(&g_battle_effect_timeline->phase2_camera.rotation[k], &rotation);
            battle_camera_execute_angle_command(g_battle_effect_timeline->phase2_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        }
    }
    if (g_battle_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && battle_camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            battle_effect_load_vector(&g_battle_effect_timeline->phase1_camera.position[k], &vector);
            battle_camera_execute_position_command(g_battle_effect_timeline->phase1_camera.command[k],
                keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            battle_effect_load_vector(&g_battle_effect_timeline->position[k], &vector);
            battle_camera_execute_position_command(
                g_battle_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            battle_effect_load_vector(&g_battle_effect_timeline->phase2_camera.position[k], &vector);
            battle_camera_execute_position_command(g_battle_effect_timeline->phase2_camera.command[k],
                keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
    if (g_battle_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && battle_camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            battle_effect_load_vector(&g_battle_effect_timeline->phase1_camera.zoom[k], &vector);
            battle_camera_execute_zoom_command(g_battle_effect_timeline->phase1_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            battle_effect_load_vector(&g_battle_effect_timeline->zoom[k], &vector);
            battle_camera_execute_zoom_command(
                g_battle_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            battle_effect_load_vector(&g_battle_effect_timeline->phase2_camera.zoom[k], &vector);
            battle_camera_execute_zoom_command(g_battle_effect_timeline->phase2_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
}

/* Reset the effect system for a battle: the effect slot chains (slot 0 the sentinel, 1-32 free),
 * the 16 secondary effect records (all on the free chain, their slot lists empty), the projectile
 * matrix translation, the render state and the effect camera. */
void battle_effect_init_system(void) {
    s32 i;
    s32 j;

    g_battle_effect_slots[0].prev = 0;
    g_battle_effect_slots[0].next = 0;
    g_battle_effect_slot_free_head = 1;
    for (i = 1; i < 32; i++) {
        g_battle_effect_slots[i].next = i + 1;
        g_battle_effect_slots[i].prev = i - 1;
    }
    g_battle_effect_slots[i].next = 0;
    g_battle_effect_slots[i].prev = i - 1;
    g_battle_effect_active_slot_head = 0;
    g_battle_effect_projectile_matrix.t[2] = 0;
    g_battle_effect_projectile_matrix.t[1] = 0;
    g_battle_effect_projectile_matrix.t[0] = 0;
    for (i = 1; i < EFFECT_SECONDARY_SLOT_COUNT; i++) {
        g_battle_effect_secondary_data[i].own_slot_id = i;
        g_battle_effect_secondary_data[i].previous_slot_id = i + 1;
        g_battle_effect_secondary_data[i].next_slot_id = i - 1;
        g_battle_effect_secondary_data[i].function_id = 0;
        g_battle_effect_secondary_data[i].allocation = 0;
    }
    g_battle_effect_secondary_data[0].previous_slot_id = 0;
    g_battle_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].next_slot_id = EFFECT_SECONDARY_SLOT_COUNT - 1;
    g_battle_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].own_slot_id = EFFECT_SECONDARY_SLOT_COUNT;
    g_battle_effect_secondary_free_head = 1;
    g_battle_effect_secondary_data[0].next_slot_id = 0;
    g_battle_effect_secondary_data[0].own_slot_id = 0;
    g_battle_effect_secondary_data[0].function_id = 0;
    g_battle_effect_secondary_data[0].allocation = 0;
    g_battle_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].previous_slot_id = 0;
    g_battle_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].function_id = 0;
    g_battle_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].allocation = 0;
    g_battle_effect_secondary_active_tail = 0;
    g_battle_effect_trap_frame_data_ptr = &g_battle_effect_trap_frame_data;
    for (i = 0; i < EFFECT_SECONDARY_RECORD_COUNT; i++) {
        g_battle_effect_secondary_data[i].active_count = 0;
        for (j = 31; j >= 0; j--) {
            g_battle_effect_secondary_data[i].slot_ids[j] = 0;
        }
    }
    battle_effect_reset_render_state();
    battle_effect_init_camera_and_screen_data();
}

/* Take a secondary effect record from the free chain and append it to the active
 * chain, with its timer and slot ids cleared, and return its id. When no record
 * is free it passes code 0x66 to the empty report hook and returns 0.
 *
 * The id is kept as an s32 byte load and a separate `slot = id & 0xff` indexes
 * the table: the target tests and indexes through the masked copy ($v0) while
 * storing and returning the raw id ($s0), which keeps the error path's `move
 * v0,s0` instead of a folded 0. Writing previous_slot_id before the next_slot_id
 * clear reproduces the target's early g_battle_effect_secondary_active_tail load. */
static u8 battle_effect_allocate_secondary_slot(void) {
    s32 id = (u8)g_battle_effect_secondary_free_head;
    s32 slot = id & 0xff;
    s32 i;

    if (slot != 0) {
        g_battle_effect_secondary_free_head = g_battle_effect_secondary_data[slot].previous_slot_id;
        g_battle_effect_secondary_data[g_battle_effect_secondary_free_head].next_slot_id = 0;
        g_battle_effect_secondary_data[slot].previous_slot_id = g_battle_effect_secondary_active_tail;
        g_battle_effect_secondary_data[slot].next_slot_id = 0;
        g_battle_effect_secondary_data[g_battle_effect_secondary_active_tail].next_slot_id = id;
        g_battle_effect_secondary_active_tail = id;
        g_battle_effect_secondary_data[slot].timer = 0;
        for (i = 31; i >= 0; i--) {
            g_battle_effect_secondary_data[slot].slot_ids[i] = 0;
        }
        return id;
    }
    main_system_report_stub(1, 0x66);
    return id;
}

/* Take secondary effect record id (1-16) off the active chain, free its work
 * block and append it to the end of the free chain headed by
 * g_battle_effect_secondary_free_head: the inverse of
 * battle_effect_allocate_secondary_slot. An id outside 1-16 only reaches the
 * empty report hook with code 0x67.
 *
 * Both chains are walked through previous_slot_id. A record removed from the
 * middle of the active chain passes its previous link to the record after it, but
 * the record before it keeps next_slot_id pointing at the freed record; only the
 * tail's next link is cleared.
 *
 * The free-chain walk index is an s32: a u16 adds an `andi 0xffff` per load. */
static void battle_effect_remove_secondary(u8 id) {
    effect_secondary_data_t* slot = &g_battle_effect_secondary_data[id];
    s32 tail;

    if (id != 0 && id < EFFECT_SECONDARY_RECORD_COUNT) {
        if (slot->allocation != 0) {
            main_heap_free(slot->allocation);
            slot->allocation = 0;
        }
        if (slot->next_slot_id != 0) {
            g_battle_effect_secondary_data[slot->next_slot_id].previous_slot_id = slot->previous_slot_id;
        } else {
            g_battle_effect_secondary_active_tail = slot->previous_slot_id;
        }
        g_battle_effect_secondary_data[g_battle_effect_secondary_active_tail].next_slot_id = 0;
        tail = g_battle_effect_secondary_free_head;
        if (tail != 0) {
            while (g_battle_effect_secondary_data[tail].previous_slot_id != 0) {
                tail = g_battle_effect_secondary_data[tail].previous_slot_id;
            }
            g_battle_effect_secondary_data[tail].previous_slot_id = id;
            slot->next_slot_id = tail;
        } else {
            g_battle_effect_secondary_free_head = id;
            slot->next_slot_id = 0;
        }
        slot->previous_slot_id = 0;
        slot->phase = EFFECT_SECONDARY_PHASE_ENDED;
        return;
    }
    main_system_report_stub(1, 0x67);
}

/* Return the phase of secondary effect record secondary_id; ENDED once the record has been
 * removed. */
effect_secondary_phase_e battle_effect_get_secondary_phase(u8 secondary_id) {
    return g_battle_effect_secondary_data[secondary_id].phase;
}

/* Ask secondary effect secondary_id to wind down: its handler runs the FINALIZING phase next. */
void battle_effect_finalize_secondary(u8 secondary_id) {
    g_battle_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_FINALIZING;
}

/* Take an effect slot from the free chain and push it on the head of the active
 * chain. Returns its id, or 0 (the null sentinel) when no slot is free; the
 * slot's life is cleared either way. */
static s32 battle_effect_alloc_slot(void) {
    u8 head = g_battle_effect_slot_free_head;
    u8 tail;
    s32 id = head;

    if (id != 0) {
        g_battle_effect_slot_free_head = g_battle_effect_slots[id].next;
        g_battle_effect_slots[g_battle_effect_slots[id].next].prev = 0;
        g_battle_effect_slots[g_battle_effect_active_slot_head].prev = head;
        tail = g_battle_effect_active_slot_head;
        g_battle_effect_slots[id].prev = 0;
        g_battle_effect_slots[id].next = tail;
        g_battle_effect_active_slot_head = head;
    }
    g_battle_effect_slots[id].life = 0;
    return id;
}

/* Unlinks slot id from the active chain and pushes it onto the free chain;
 * slot 0 is the null sentinel and is ignored. */
static void battle_effect_free_slot(u8 id) {
    u8 free_head;

    if (id == 0) {
        return;
    }
    g_battle_effect_slots[g_battle_effect_slot_free_head].prev = id;
    if (g_battle_effect_slots[id].prev != 0) {
        g_battle_effect_slots[g_battle_effect_slots[id].prev].next = g_battle_effect_slots[id].next;
    } else {
        g_battle_effect_active_slot_head = g_battle_effect_slots[id].next;
    }
    g_battle_effect_slots[g_battle_effect_slots[id].next].prev = g_battle_effect_slots[id].prev;
    free_head = g_battle_effect_slot_free_head;
    g_battle_effect_slots[id].prev = 0;
    g_battle_effect_slots[id].next = free_head;
    g_battle_effect_slot_free_head = id;
}

/* Start secondary effect animation_id for the caster and first target of the
 * init record and return its id. parameter is the animation's own: an
 * element mask, a sprite palette or a duration. */
s32 battle_effect_init_secondary(s32 animation_id, s32 parameter, effect_secondary_init_t* source) {
    s32 secondary_id = battle_effect_allocate_secondary_slot();

    g_battle_effect_secondary_data[secondary_id].animation_id = animation_id;
    g_battle_effect_secondary_data[secondary_id].timer = 0;
    g_battle_effect_secondary_data[secondary_id].parameter = parameter;
    g_battle_effect_secondary_data[secondary_id].function_id
        = g_battle_effect_secondary_animations[g_battle_effect_secondary_data[secondary_id].animation_id].function_id;
    g_battle_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_battle_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_battle_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_battle_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Start the current effect target's secondary effect: the low half of
 * animation selects the animation id (and its function id), the high half is
 * stored alongside it. Returns its id; phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 battle_effect_init_projectile_secondary(s32 animation, effect_secondary_init_t* source) {
    u8 secondary_id = battle_effect_allocate_secondary_slot();

    g_battle_effect_secondary_data[secondary_id].animation_id = animation;
    g_battle_effect_secondary_data[secondary_id].timer = 0;
    g_battle_effect_secondary_data[secondary_id].parameter = animation >> 16;
    g_battle_effect_secondary_data[secondary_id].function_id
        = g_battle_effect_secondary_animations[g_battle_effect_secondary_data[secondary_id].animation_id].function_id;
    g_battle_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_battle_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_battle_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_battle_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_battle_effect_secondary_data[secondary_id].target_count = source->target_count;
    return secondary_id;
}

/* Returns the effect_element_index_e of an element mask: 0 for no
 * element, otherwise the position of the first table mask it meets (9 when it
 * meets none). */
s32 battle_effect_get_element_index(s32 elements) {
    s32 i;

    if (elements != 0) {
        for (i = 0; i < 9; i++) {
            if ((elements & g_battle_effect_element_masks[i]) != 0) {
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
s32 battle_effect_init_weapon_hit_secondary(s32 elements, effect_secondary_init_t* source, s32 unused_is_bow) {
    u8 secondary_id;

    if (source->target_count != 0) {
        elements = battle_effect_get_element_index(elements);
        secondary_id = battle_effect_allocate_secondary_slot();
        switch (source->targets[0].data.unit.result_animation) {
        case EFFECT_HIT_NORMAL:
        case EFFECT_HIT_CRITICAL:
        case EFFECT_HIT_MISSED:
        case EFFECT_HIT_NO_EFFECT:
            g_battle_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_WEAPON_HIT;
            break;
        case EFFECT_HIT_BREAK:
        case EFFECT_HIT_BREAK_ALT:
            g_battle_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK;
            break;
        case EFFECT_HIT_GOLEM:
            g_battle_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_GOLEM;
            break;
        }
        g_battle_effect_secondary_data[secondary_id].parameter = elements;
        g_battle_effect_secondary_data[secondary_id].function_id
            = g_battle_effect_secondary_animations[g_battle_effect_secondary_data[secondary_id].animation_id]
                  .function_id;
        g_battle_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
        g_battle_effect_secondary_data[secondary_id].target.record = source->targets[0];
        g_battle_effect_secondary_data[secondary_id].timer = 0;
        g_battle_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    } else {
        secondary_id = 0;
    }
    return secondary_id;
}

/* Start the current effect target's fall-dust secondary effect at the
 * given position and return its id. Phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 battle_effect_init_fall_dust_secondary(effect_secondary_init_t* source, VECTOR* position) {
    s32 secondary_id = battle_effect_allocate_secondary_slot();

    g_battle_effect_secondary_data[secondary_id].timer = 0;
    g_battle_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_FALL_DUST;
    g_battle_effect_secondary_data[secondary_id].function_id
        = g_battle_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_FALL_DUST]
              .function_id; /* array access keeps the load after the animation store */
    g_battle_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_battle_effect_secondary_data[secondary_id].target.position.x = position->vx >> 4;
    g_battle_effect_secondary_data[secondary_id].target.position.z = position->vy >> 4;
    g_battle_effect_secondary_data[secondary_id].target.position.y = position->vz >> 4;
    g_battle_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Timer and animation use field-address offsets to retain their store order. */

/* Start EFFECT_SECONDARY_ANIMATION_ENDING in a fresh secondary effect
 * record and return its id. Its null handler only steps through the phases, so
 * the record lasts two more frames. The arrow handler starts it when the arrow
 * lands without a hit. */
static s32 battle_effect_end_secondary_on_current_unit(void) {
    s32 secondary_id = battle_effect_allocate_secondary_slot();
    s32 record_offset = secondary_id * sizeof(effect_secondary_data_t);
    u8 function_id;

    *(s16*)((u8*)&g_battle_effect_secondary_data[0].timer + record_offset) = 0;
    *(u16*)((u8*)&g_battle_effect_secondary_data[0].animation_id + record_offset) = EFFECT_SECONDARY_ANIMATION_ENDING;
    function_id = g_battle_effect_ending_secondary_function_id;
    g_battle_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_battle_effect_secondary_data[secondary_id].function_id = function_id;
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
static void battle_effect_draw_projectile_model(
    SVECTOR* spin, VECTOR* position, SVECTOR* angles, VECTOR* scale, s32 parameter) {
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
    model = g_battle_effect_model_files[parameter & 0xff];
    scratch = (VECTOR*)getScratchAddr(0);
    vertices = (SVECTOR*)((u8*)model->objects + model->objects[0].vertex_offset);
    count = model->objects[0].vertex_count;
    for (i = 0; i < count; i++) {
        gte_RotTrans(&vertices[i], &scratch[i], &scratch[i].pad);
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
        gte_RotTrans(&point, &scratch[i], &scratch[i].pad);
    }
    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    for (i = 0; i < count; i++) {
        copyVector(&point, &scratch[i]);
        gte_RotTrans(&point, &scratch[i], &scratch[i].pad);
    }
    command_count = battle_effect_init_model_sections(model, 0);
    gt3_index = 0;
    g4_index = 0;
    g3_index = 0;
    switch (parameter & 0xff) {
    case EFFECT_MODEL_ARROW: {
        effect_arrow_buffer_t* work;
        POLY_G4* quad;
        POLY_G3* tri;

        work = g_battle_effect_current_secondary->allocation;
        for (i = 0; i < command_count;) {
            battle_effect_decode_next_model_command(&command);
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G4) {
                quad = &work->g4[g_battle_effect_buffer_index][g4_index];
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
                    addPrim(g_battle_effect_polygon_depth_data + z, quad);
                }
            }
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_battle_effect_buffer_index][g3_index];
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
                    addPrim(g_battle_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_STONE: {
        effect_stone_buffer_t* work;
        POLY_G3* tri;

        work = g_battle_effect_current_secondary->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            battle_effect_decode_next_model_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_battle_effect_buffer_index][j];
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
                    addPrim(g_battle_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_REFLECT: {
        effect_reflect_buffer_t* work;
        POLY_GT3* tri;

        work = g_battle_effect_current_secondary->allocation;
        /* g4_index is reused here as the limit on decoded commands. */
        g4_index = g_battle_effect_current_secondary->timer * 3;
        if (command_count < g4_index) {
            g4_index = command_count;
        }
        for (i = 0; i < g4_index;) {
            battle_effect_decode_next_model_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_GT3_37) {
                tri = &work->prims[g_battle_effect_buffer_index][gt3_index];
                tri->tpage = 0x3f;
                tri->clut = g_battle_effect_current_secondary->own_slot_id + 0x7eff;
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
                    ot = g_battle_effect_polygon_depth_data;
                    addPrim(ot, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_SHURIKEN: {
        effect_shuriken_buffer_t* work;
        POLY_G3* tri;

        work = g_battle_effect_current_secondary->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            battle_effect_decode_next_model_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_battle_effect_buffer_index][j];
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
                    addPrim(g_battle_effect_polygon_depth_data + z, tri);
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
static void battle_effect_draw_item_drop_sprite(VECTOR* origin, s32 item_id, s32 angle) {
    SPRT sprite;
    SVECTOR local;
    VECTOR position;
    s32 sxy;
    s32 flag;
    effect_slot_t* slot = &g_battle_effect_slots[g_battle_effect_current_secondary->slot_ids[0]];
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

    gfx_set_item_icon_sprite(&sprite, item_id);
    prim = &slot->quads[g_battle_effect_buffer_index];
    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    copyVector(&local, origin);
    RotTrans(&local, &position, &position.pad);
    depth = (position.vz >> 2) - 8;
    if (depth < EFFECT_OT_DEPTH_MIN) {
        depth = EFFECT_OT_DEPTH_MIN;
    }
    if (depth >= EFFECT_OT_DEPTH_LIMIT) {
        depth = EFFECT_OT_DEPTH_MAX;
    }
    g_battle_effect_matrix.t[0] = position.vx;
    g_battle_effect_matrix.t[1] = position.vy;
    cosine = rcos(angle);
    sine = rsin(angle);
    zoom = g_battle_camera_zoom.vx;
    scale_y = g_battle_camera_zoom.vy;
    g_battle_effect_matrix.m[0][0] = (cosine * zoom) >> 12;
    g_battle_effect_matrix.m[1][1] = (cosine * scale_y) >> 12;
    g_battle_effect_matrix.m[0][1] = (-sine * scale_y) >> 12;
    g_battle_effect_matrix.m[1][0] = (sine * zoom) >> 12;
    SetRotMatrix(&g_battle_effect_matrix);
    SetTransMatrix(&g_battle_effect_matrix);
    half_w = -sprite.w / 2;
    right = half_w + sprite.w;
    half_h = -sprite.h / 2;
    bottom = half_h + sprite.h;
    g_battle_effect_corner_bottom_left.vx = half_w;
    g_battle_effect_corner_top_left.vx = half_w;
    g_battle_effect_corner_bottom_right.vx = right;
    g_battle_effect_corner_top_right.vx = right;
    g_battle_effect_corner_top_right.vy = half_h;
    g_battle_effect_corner_top_left.vy = half_h;
    g_battle_effect_corner_bottom_right.vy = bottom;
    g_battle_effect_corner_bottom_left.vy = bottom;
    RotTransPers3(&g_battle_effect_corner_top_left, &g_battle_effect_corner_top_right,
        &g_battle_effect_corner_bottom_left, (s32*)&prim->x0, (s32*)&prim->x1, (s32*)&prim->x2, &sxy, &flag);
    RotTransPers(&g_battle_effect_corner_bottom_right, (s32*)&prim->x3, &sxy, &flag);
    AddPrim(g_battle_effect_polygon_depth_data + depth, prim);
}

/* Quadratic height term used by the projectile trajectory calculation;
 * this is not the complete trajectory.
 * distance is the attacker-to-target distance scaled by ONE. */
static s32 battle_effect_calculate_arc_height_term(s32 distance) {
    s32 gravity;

    distance >>= 6;
    gravity = g_battle_effect_gravity_modifier;
    return gravity / 2 - (distance * distance) / (gravity >> 11);
}

/* Solve the two launch slopes of an arcing projectile toward the target.
 *
 * Lowers the target's aim height from half, to three quarters, to zero of its
 * sprite height until both the arc apex and the target tile floor clear it,
 * storing the chosen height back into the target endpoint. Returns 0 when the
 * target is at zero distance or no height clears. */
static s32 battle_effect_calculate_arcing_trajectory(battle_arc_query_t* query, s32* arc_a, s32* arc_b) {
    map_tile_t* tile;
    s32 tile_height;
    s32 floor;
    s32 peak;
    s32 height;
    s32 limit;
    s32 root;
    s32 gravity;
    s32 distance;
    s32 result = 0;

    tile = battle_map_get_tile_data_ptr_from_battle_id(query->target.unit_id);
    tile_height = tile->height + tile->depth_half_height.depth;
    floor = (tile_height * 3) << 14;
    peak = battle_effect_calculate_arc_height_term(query->delta.distance);
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
        gravity = g_battle_effect_gravity_modifier >> 6;
        distance = query->delta.distance >> 6;
        root = SquareRoot12((gravity - (limit >> 5)) * gravity - distance * distance);
        *arc_a = ((root + g_battle_effect_gravity_modifier) << 8) / (query->delta.distance >> 4);
        *arc_b = ((g_battle_effect_gravity_modifier - root) << 8) / (query->delta.distance >> 4);
        result = 1;
    }
    return result;
}

/*
 * Evaluate the arcing-weapon trajectory term a * b - (1 + a^2) * b^2 /
 * (2 * g_battle_effect_gravity_modifier) in 20.12 fixed point.
 *
 * This is the height of a ballistic arc after a horizontal distance b
 * (`progress`), where a (`height`) is the tangent of the launch pitch in Q12:
 * the callers take the pitch itself as ratan2(height, ONE).
 *
 * Both operands are pre-shifted by range so the squares stay within 32 bits.
 * g_battle_effect_gravity_modifier is the trajectory gravity modifier
 * (0xa8000).
 */
static s32 battle_effect_calculate_arc_trajectory_term(s32 height, s32 progress) {
    s32 divisor = (g_battle_effect_gravity_modifier * 2) >> 12;
    s32 height_sq;
    s32 progress_sq;
    s32 linear;

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
static void battle_effect_find_projectile_xyz_along_trajectory(const VECTOR* src, s32 total, s32 current, VECTOR* dst) {
    s32 progress = (current << 8) / (total >> 4);

    dst->vx = (progress * src->vx) >> 12;
    dst->vy = (progress * src->vy) >> 12;
    dst->vz = (progress * src->vz) >> 12;
}

/* Launch speed of every arcing projectile: SquareRoot12 of g_battle_effect_gravity_modifier times
 * g_battle_effect_gravity_scale. With the retail values (0xa8000 and 0x53a) it is a constant,
 * about 7.4 in Q12. */
static s32 battle_effect_calculate_launch_velocity(void) {
    return SquareRoot12((g_battle_effect_gravity_modifier * g_battle_effect_gravity_scale) >> 12);
}

/*
 * List every present unit other than `first` and `second` that is not Jumping
 * and is not a mount carrying a rider, with its tile center and height and its
 * sprite height, into a record at the start of the scratchpad returned via `out`.
 */
static void battle_effect_list_valid_unit_targeting_data(s32 first, s32 second, effect_obstacle_unit_list_t** out) {
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
static s32 battle_effect_test_position_for_obstacle(
    effect_obstacle_unit_list_t* list, VECTOR* position, s32* out_unit, map_tile_coordinates_t* tile_ref) {
    map_tile_t* tile;
    s32 ceiling;
    s32 height;
    s32 y;
    s32 i;

    *out_unit = -1;
    tile_ref->x = position->vx / MAP_TILE_SIZE;
    tile_ref->y = position->vz / MAP_TILE_SIZE;
    tile = battle_map_get_tile_data_pointer(tile_ref->x, tile_ref->y, 0);
    if (tile->ceiling_depth_and_marks.ceiling_depth == 0) {
        ceiling = tile->height;
    } else {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * 12) && y < -((height - ceiling) * 12)) {
        tile_ref->layer = 0;
        g_battle_effect_trajectory_tile_flags = tile->flags._unknown_00;
        return 1;
    }

    tile = battle_map_get_tile_data_pointer(tile_ref->x, tile_ref->y, 1);
    ceiling = 1;
    if (tile->ceiling_depth_and_marks.ceiling_depth != 0) {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * 12) && y < -((height - ceiling) * 12)) {
        tile_ref->layer = 1;
        g_battle_effect_trajectory_tile_flags = tile->flags._unknown_00;
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
 * g_battle_effect_arctan_angle_mod; the per-step advance is the horizontal
 * component of the gravity impulse. Each step rotates the local (progress,
 * -trajectory term) offset into world space through the yaw matrix and tests
 * the result against the obstacle list, exactly as
 * battle_effect_trace_projectile_path does for straight shots. On a clear
 * path g_battle_effect_trajectory_step_count holds the full step count and the result is 1; on a hit it
 * holds the steps taken and the result is 0.
 *
 * `hit` is deliberately left uninitialized: the target reads $s6 after a
 * zero-step loop without ever writing it.
 */
static s32 battle_effect_trace_arc_trajectory_path(
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
    g_battle_effect_arctan_angle_mod = angle;
    progress = 0;
    velocity = battle_effect_calculate_launch_velocity() >> 8;
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
        term = battle_effect_calculate_arc_trajectory_term(*height, progress);
        step.vx = progress >> 12;
        step.vy = -term >> 12;
        RotTrans(&step, &position, &position.pad);
        hit = battle_effect_test_position_for_obstacle(
            obstacles, &position, &g_battle_effect_trajectory_hit_unit_id, &g_battle_effect_trajectory_tile);
        if (hit != 0) {
            break;
        }
    }

    setVector(&g_battle_effect_trajectory_step, position.vx - previous.vx, position.vy - previous.vy,
        position.vz - previous.vz);
    copyVector(&g_battle_effect_trajectory_position, &position);
    if (hit != 0) {
        g_battle_effect_trajectory_step_count = i;
        return 0;
    }
    g_battle_effect_trajectory_step_count = steps;
    return 1;
}

/* Test whether either arcing projectile path reaches the target unit.
 *
 * The arc counterpart of battle_effect_check_direct_trajectory_between_units: the
 * source is raised by two thirds of its sprite height, battle_effect_calculate_arcing_trajectory yields
 * the two candidate arcs for the offset, and each is traced against the
 * other units' positions. Returns 1 when an arc is clear. */
s32 battle_effect_check_arc_trajectory_between_units(u8 source_id, u8 target_id) {
    SVECTOR rotation;
    battle_arc_query_t query;
    effect_obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;

    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    query.target.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id);
    battle_effect_list_valid_unit_targeting_data(source_id, target_id, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &query.source.position);
    query.source.position.vy -= query.source.height;
    battle_unit_get_tile_center_and_height_by_battle_id(target_id, &query.target.position);
    query.delta.x = query.target.position.vx - query.source.position.vx;
    query.delta.z = query.target.position.vz - query.source.position.vz;
    query.delta.y = query.source.position.vy - query.target.position.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    rotation.vy = ratan2(-query.delta.z, query.delta.x);
    if (battle_effect_calculate_arcing_trajectory(&query, &arc_a, &arc_b) != 0
        && (battle_effect_trace_arc_trajectory_path(
                &query.source.position, &arc_a, &query.delta.distance, &rotation, obstacles)
                != 0
            || battle_effect_trace_arc_trajectory_path(
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
 * Both candidate arcs from battle_effect_calculate_arcing_trajectory are
 * traced, arc_b first; a clear arc returns target_id. Otherwise an arc blocked
 * by a unit of another team than the source wins (arc_b's first) and returns
 * that unit. Failing that, arc_a's trace is kept and its blocker (or -1) is
 * returned, except when only arc_a was blocked by a unit: then arc_b's
 * terrain-blocked result is restored and -1 returned. g_battle_effect_arc_trajectory_height receives the
 * chosen arc height. `unused` reserves the target's 16 otherwise unused frame bytes.
 */
s32 battle_effect_set_and_validate_arc_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id) {
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

    g_battle_effect_trajectory_source_id = source_id;
    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        query.target.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id);
    } else {
        query.target.height = 0;
    }
    battle_effect_list_valid_unit_targeting_data(source_id, -1, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_battle_effect_trajectory_origin);
    g_battle_effect_trajectory_origin.vy -= query.source.height;
    if (target_id == -1) {
        tile = battle_map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_battle_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_battle_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_battle_effect_trajectory_destination.vy = -((tile->height + tile->depth_half_height.depth) * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_battle_effect_trajectory_destination);
    }
    query.delta.x = g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx;
    query.delta.z = g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz;
    query.delta.y = g_battle_effect_trajectory_origin.vy - g_battle_effect_trajectory_destination.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    arc.vy = ratan2(-query.delta.z, query.delta.x);
    if (battle_effect_calculate_arcing_trajectory(&query, &arc_a, &arc_b) == 0) {
        arc_b = ONE;
        arc_a = ONE;
    }

    hit = battle_effect_trace_arc_trajectory_path(
        &g_battle_effect_trajectory_origin, &arc_b, &query.delta.distance, &arc, obstacles);
    first_unit = g_battle_effect_trajectory_hit_unit_id;
    first_step = g_battle_effect_trajectory_step;
    first_position = g_battle_effect_trajectory_position;
    first_count = g_battle_effect_trajectory_step_count;
    first_other = -1;
    if (g_battle_effect_trajectory_hit_unit_id == -1) {
        first_other = g_battle_effect_trajectory_tile_flags;
    }
    if (hit != 0) {
        height = arc_b;
        result = target_id;
    } else {
        second_hit = battle_effect_trace_arc_trajectory_path(
            &g_battle_effect_trajectory_origin, &arc_a, &query.delta.distance, &arc, obstacles);
        second_unit = g_battle_effect_trajectory_hit_unit_id;
        second_step = g_battle_effect_trajectory_step;
        second_position = g_battle_effect_trajectory_position;
        second_count = g_battle_effect_trajectory_step_count;
        second_other = -1;
        if (g_battle_effect_trajectory_hit_unit_id == -1) {
            second_other = g_battle_effect_trajectory_tile_flags;
        }
        if (second_hit != 0) {
            height = arc_a;
            result = target_id;
        } else {
            stats = battle_unit_get_stats_from_battle_id(source_id);
            if (first_unit != -1
                && (other = battle_unit_get_stats_from_battle_id(first_unit),
                    (stats->team_flags & BATTLE_TEAM_MASK) != (other->initial_team_flags & BATTLE_TEAM_MASK))) {
                g_battle_effect_trajectory_step = first_step;
                g_battle_effect_trajectory_position = first_position;
                height = arc_b;
                g_battle_effect_trajectory_hit_unit_id = first_unit;
                result = first_unit;
                g_battle_effect_trajectory_step_count = first_count;
            } else if (second_unit != -1
                && (other = battle_unit_get_stats_from_battle_id(second_unit),
                    (stats->team_flags & BATTLE_TEAM_MASK) != (other->initial_team_flags & BATTLE_TEAM_MASK))) {
                height = arc_a;
                g_battle_effect_trajectory_step_count = second_count;
                g_battle_effect_trajectory_hit_unit_id = second_unit;
                result = second_unit;
            } else if (second_unit == -1) {
                height = arc_a;
                g_battle_effect_trajectory_step_count = second_count;
                g_battle_effect_trajectory_hit_unit_id = second_unit;
                g_battle_effect_trajectory_tile_flags = second_other;
                result = -1;
            } else if (first_unit != -1) {
                height = arc_a;
                g_battle_effect_trajectory_step_count = second_count;
                g_battle_effect_trajectory_tile_flags = second_other;
                g_battle_effect_trajectory_hit_unit_id = second_unit;
                result = second_unit;
            } else {
                g_battle_effect_trajectory_step = first_step;
                g_battle_effect_trajectory_position = first_position;
                height = arc_b;
                g_battle_effect_trajectory_hit_unit_id = first_unit;
                result = -1;
                g_battle_effect_trajectory_tile_flags = first_other;
                g_battle_effect_trajectory_step_count = first_count;
            }
        }
    }
    g_battle_effect_arc_trajectory_height = height;
    return result;
}

/* Wrap the vertical shot angle the last trajectory test stored
 * (g_battle_effect_arctan_angle_mod) into [-0x800, 0x800] and bucket it at
 * 0x2aa (60 degrees): 0 below -0x2aa, 1 from -0x2aa to 0x2aa, 2 above.
 * battle_unit_select_weapon_attack_animation uses it as the bow's column of
 * g_battle_weapon_attack_animations. */
s32 battle_effect_get_bow_shot_angle(void) {
    s16 angle = g_battle_effect_arctan_angle_mod;
    if (angle >= 0x801) {
        g_battle_effect_arctan_angle_mod = angle - ONE;
    }
    {
        s16 angle_after_hi = g_battle_effect_arctan_angle_mod;
        if (angle_after_hi < -0x800) {
            g_battle_effect_arctan_angle_mod = angle_after_hi + ONE;
        }
    }
    {
        s16 wrapped = g_battle_effect_arctan_angle_mod;
        if (wrapped < 0x2AB) {
            return wrapped >= -0x2AA;
        }
        return 2;
    }
}

/*
 * Step a projectile from `origin` along `delta` in 0x2000 increments of the
 * `distance` total, testing each position against the obstacle list. Stores
 * the last step's movement and the final position, sets g_battle_effect_trajectory_step_count to a
 * quarter of the steps taken, and returns 1 when the path is clear or 0 when
 * an obstacle stopped it. The `step` variable keeps the target's register
 * divide; a literal 0x2000 folds to a shift.
 */
static s32 battle_effect_trace_projectile_path(
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
        battle_effect_find_projectile_xyz_along_trajectory(delta, *distance, progress, &position);
        position.vx += origin->vx;
        position.vy += origin->vy;
        position.vz += origin->vz;
        hit = battle_effect_test_position_for_obstacle(
            obstacles, &position, &g_battle_effect_trajectory_hit_unit_id, &g_battle_effect_trajectory_tile);
        if (hit != 0) {
            break;
        }
    }
    setVector(&g_battle_effect_trajectory_step, position.vx - previous.vx, position.vy - previous.vy,
        position.vz - previous.vz);
    copyVector(&g_battle_effect_trajectory_position, &position);
    if (hit != 0) {
        g_battle_effect_trajectory_step_count = i / 4;
        return 0;
    }
    g_battle_effect_trajectory_step_count = steps / 4;
    return 1;
}

/*
 * Trace a projectile path from one unit's upper body to another's.
 *
 * Both endpoints are raised by two thirds of the unit's sprite height (12 when
 * the target has none). `unused_20` and `unused_38` reproduce unreferenced
 * frame slots the target reserves between the vectors.
 */
s32 battle_effect_check_direct_trajectory_between_units(u8 source_id, u8 target_id) {
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
    battle_effect_list_valid_unit_targeting_data(source_id, target_id, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &origin);
    origin.vy -= source_height;
    battle_unit_get_tile_center_and_height_by_battle_id(target_id, &destination);
    destination.vy -= target_height;
    setVector(&delta, destination.vx - origin.vx, destination.vy - origin.vy, destination.vz - origin.vz);
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    return battle_effect_trace_projectile_path(&delta, &origin, &distance, obstacles);
}

/*
 * Traces a straight projectile path from a source unit to either a target
 * unit or a bare tile, and reports whether the flight is obstructed.
 *
 * Both endpoints are raised by two thirds of their sprite height (12 when the
 * target has none), as in battle_effect_check_direct_trajectory_between_units;
 * the tile case instead centers on the tile at tile_position (x, layer, y) and
 * sits on its surface, as in battle_effect_set_arrow_trajectory.  The extra
 * work over those two is the launch angle: ratan2 of the vertical delta
 * against the horizontal distance, biased by 0x400 (a quarter turn) into
 * g_battle_effect_arctan_angle_mod.  Returns target_id when the path is clear
 * and the blocking unit from g_battle_effect_trajectory_hit_unit_id when it is not.
 */
s32 battle_effect_check_direct_trajectory_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    map_tile_t* tile;
    effect_obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    g_battle_effect_trajectory_source_id = source_id;
    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 2 / 3;
        if (target_height == 0) {
            target_height = 12;
        }
    }
    battle_effect_list_valid_unit_targeting_data(source_id, -1, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_battle_effect_trajectory_origin);
    g_battle_effect_trajectory_origin.vy -= source_height;
    if (target_id == -1) {
        tile = battle_map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_battle_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_battle_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_battle_effect_trajectory_destination.vy = -(tile->height * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_battle_effect_trajectory_destination);
        g_battle_effect_trajectory_destination.vy -= target_height;
    }
    setVector(&delta, g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx,
        g_battle_effect_trajectory_destination.vy - g_battle_effect_trajectory_origin.vy,
        g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz);
    g_battle_effect_arctan_angle_mod = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    if (battle_effect_trace_projectile_path(&delta, &g_battle_effect_trajectory_origin, &distance, obstacles) != 0) {
        return target_id;
    }
    return g_battle_effect_trajectory_hit_unit_id;
}

/* Sets an arrow's source, destination and flight duration.
 *
 * The projectile origin is the first unit raised by two thirds of its
 * sprite height, and the destination is the second unit raised by three
 * quarters of its height, or, when target_id is -1, to the center of the tile
 * at tile_position (x, layer, y) on its surface. g_battle_effect_trajectory_step_count receives the
 * flight duration derived from the distance, as in
 * battle_effect_check_direct_trajectory_between_units / battle_effect_trace_projectile_path.
 * `delta` reproduces the target's stores to an otherwise unused stack vector.
 */
void battle_effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    map_tile_t* tile;
    s32 source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    s32 target_height;

    if (target_id != -1) {
        target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 3 / 4;
    }
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_battle_effect_trajectory_origin);
    g_battle_effect_trajectory_origin.vy -= source_height;
    if (target_id == -1) {
        tile = battle_map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_battle_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_battle_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_battle_effect_trajectory_destination.vy = -(tile->height * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_battle_effect_trajectory_destination);
        g_battle_effect_trajectory_destination.vy -= target_height;
    }
    setVector(&delta, g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx,
        g_battle_effect_trajectory_destination.vy - g_battle_effect_trajectory_origin.vy,
        g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz);
    g_battle_effect_trajectory_step_count
        = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE) / 8 >> 12;
}

/* Keep the trap animation table as u8[]: another element type introduces an
 * 8-byte compiler frame and changes the load-delay and return slots. */

/* Start the animation script of trap group on slot: reset the color and
 * script state and store the 16-bit CLUT value (callers pass CLUTs of
 * MAP_CLUT_ROW_WEAPONS) at 0x88. */
static void battle_effect_init_trap_animation(s32 group, u16 clut, effect_slot_t* slot) {
    u32 offset;

    slot->clut = clut;
    offset = *(
        u16*)(g_battle_effect_trap_animation_data + (g_battle_effect_trap_particle_configs[group].trap_animation << 1));
    slot->color.b = 0x80;
    slot->color.g = 0x80;
    slot->color.r = 0x80;
    slot->step_index = 0;
    slot->hold = 0;
    slot->angle = 0;
    slot->script = (effect_anim_script_t*)(g_battle_effect_trap_animation_data + offset);
}

/* Spawn this frame's share of a particle group's effect slots, drawn with CLUT
 * clut_index of the weapons row.
 *
 * The group spawns while the secondary effect's timer is inside its spawn
 * frames. Its spawns_per_frame caps how many slots one frame may start; the
 * secondary effect's active_count counts the live slots and stops the
 * spawning at the group's total count. */
static void battle_effect_start_group(s32 group, s32 clut_index) {
    effect_slot_t* slot;
    s32 spawned;
    s32 i;
    u8 id;

    if (g_battle_effect_trap_particle_configs[group].count > g_battle_effect_current_secondary->active_count
        && g_battle_effect_current_secondary->timer >= g_battle_effect_trap_particle_configs[group].spawn_start_frame
        && g_battle_effect_current_secondary->timer < g_battle_effect_trap_particle_configs[group].spawn_end_frame) {
        for (spawned = 0; spawned < g_battle_effect_trap_particle_configs[group].spawns_per_frame; spawned++) {
            for (i = 0; i < g_battle_effect_trap_particle_configs[group].count; i++) {
                if (g_battle_effect_current_secondary->slot_ids[i] == 0) {
                    break;
                }
            }
            id = battle_effect_alloc_slot();
            g_battle_effect_current_secondary->slot_ids[i] = id;
            if (id == 0) {
                break;
            }
            slot = &g_battle_effect_slots[g_battle_effect_current_secondary->slot_ids[i]];
            slot->life
                = battle_effect_spawn_particle_motion(&g_battle_effect_trap_particle_configs[group], &slot->motion);
            battle_effect_init_trap_animation(group, clut_index + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
            g_battle_effect_current_secondary->active_count++;
            if (g_battle_effect_current_secondary->active_count == g_battle_effect_trap_particle_configs[group].count) {
                break;
            }
        }
    }
}

/* Advance the current secondary effect's particles: every effect slot in the
 * first count entries of slot_ids, count being the particle group's. A slot that
 * has finished is freed and active_count drops by one. */
static void battle_effect_update_active_slots(s32 group) {
    s32 i;
    s32 id;

    for (i = 0; i < g_battle_effect_trap_particle_configs[group].count; i++) {
        id = g_battle_effect_current_secondary->slot_ids[i];
        if (id != 0 && battle_effect_update_slot(id) == 0) {
            battle_effect_free_slot(id);
            g_battle_effect_current_secondary->slot_ids[i] = 0;
            g_battle_effect_current_secondary->active_count--;
        }
    }
}

/* Secondary effect function 0x00 in g_battle_effect_secondary_handlers: returns 0, so a record
 * with no function is removed on its first update. */
s32 battle_effect_none_secondary_handler(void) {
    return 0;
}

/* An empty routine among the secondary-effect handlers. Nothing references it, not
 * even g_battle_effect_secondary_handlers. */
void battle_noop_801b0ff4(void) {
}

/*
 * Secondary-effect handler for bow and crossbow arrows.
 *
 * Phase 1 derives the arrow's heading, launch pitch and per-step gravity from
 * the trajectory origin/destination, builds its rotation matrix and allocates
 * the polygon work buffer. Phase 2 advances one step along the arc, orients
 * the arrow along the local slope and draws it with battle_effect_draw_projectile_model; the last
 * step or a positive height ends the flight. Phase 3 returns 0. An unknown
 * phase returns `result` uninitialized, as the target returns $s6 unwritten.
 */
s32 battle_effect_arrow_arc_secondary_handler(void) {
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

    switch (g_battle_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        delta.vx = g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx;
        delta.vz = g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz;
        delta.vy = g_battle_effect_trajectory_origin.vy - g_battle_effect_trajectory_destination.vy;
        g_battle_effect_arrow_arc_length = SquareRoot12((delta.vx * delta.vx + delta.vz * delta.vz) * ONE);
        g_battle_effect_arrow_arc_yaw = ratan2(-delta.vz, delta.vx);
        result = 1;
        g_battle_effect_arrow_arc_caster_relative_yaw = g_battle_effect_arrow_arc_yaw
            - (((battle_unit_get_facing_quadrant_by_runtime_id(
                     g_battle_effect_current_secondary->caster.data.unit.runtime_id)
                    + 1)
                   & 3)
                << 10);
        g_battle_effect_arrow_arc_height = g_battle_effect_arc_trajectory_height;
        g_battle_effect_arrow_arc_launch_angle = ratan2(g_battle_effect_arc_trajectory_height, ONE);
        g_battle_effect_arrow_arc_distance = 0;
        velocity = battle_effect_calculate_launch_velocity() >> 8;
        g_battle_effect_arrow_arc_distance_step = (rcos(g_battle_effect_arrow_arc_launch_angle) * velocity) >> 4;
        g_battle_effect_projectile_step_count = g_battle_effect_trajectory_step_count;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = g_battle_effect_arrow_arc_yaw;
        RotMatrix(&rotation, &g_battle_effect_projectile_matrix);
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        if (g_battle_effect_current_secondary->allocation != 0) {
            main_heap_free(g_battle_effect_current_secondary->allocation);
        }
        work = main_heap_alloc(0xc88);
        g_battle_effect_current_secondary->allocation = work;
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
        previous = -battle_effect_calculate_arc_trajectory_term(
            g_battle_effect_arrow_arc_height, g_battle_effect_arrow_arc_distance);
        g_battle_effect_arrow_arc_distance += g_battle_effect_arrow_arc_distance_step;
        term = -battle_effect_calculate_arc_trajectory_term(
            g_battle_effect_arrow_arc_height, g_battle_effect_arrow_arc_distance);
        next = -battle_effect_calculate_arc_trajectory_term(g_battle_effect_arrow_arc_height,
            g_battle_effect_arrow_arc_distance + g_battle_effect_arrow_arc_distance_step);
        setVector(&angles, 0, g_battle_effect_arrow_arc_yaw,
            ratan2(next - previous, g_battle_effect_arrow_arc_distance_step * 2) + 0x400);
        SetRotMatrix(&g_battle_effect_projectile_matrix);
        SetTransMatrix(&g_battle_effect_projectile_matrix);
        rotation.vy = term >> 12;
        rotation.vz = 0;
        rotation.vx = g_battle_effect_arrow_arc_distance >> 12;
        RotTrans(&rotation, &delta, &delta.pad);
        setVector(&position, g_battle_effect_trajectory_origin.vx + delta.vx,
            g_battle_effect_trajectory_origin.vy + delta.vy, g_battle_effect_trajectory_origin.vz + delta.vz);
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        setVector(&rotation, g_battle_effect_trajectory_origin.vx + delta.vx,
            g_battle_effect_trajectory_origin.vy + delta.vy, g_battle_effect_trajectory_origin.vz + delta.vz);
        RotTrans(&rotation, &delta, &delta.pad);
        scale.vz = 0x40;
        scale.vy = 0x40;
        scale.vx = 0x40;
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
        spin.vz = 0;
        spin.vy = 0;
        spin.vx = 0;
        battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        if (g_battle_effect_projectile_step_count == 2 && g_battle_effect_trajectory_hit_unit_id != -1
            && g_battle_effect_current_secondary->target_count != 0) {
            battle_unit_call_bow_hardcoding_by_runtime_id(
                g_battle_effect_current_secondary->caster.data.unit.runtime_id,
                g_battle_effect_current_secondary->target.record.data.unit.runtime_id);
        }
        if (--g_battle_effect_projectile_step_count == 0 || position.vy >= 0) {
            g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
            flags = g_battle_effect_current_secondary->target.record.data.unit.result_animation;
            if (flags == EFFECT_HIT_MISSED || flags - 4 < 2U || g_battle_effect_trajectory_hit_unit_id == -1) {
                g_battle_unknown_801b8b98 = 0x80000000;
                battle_effect_end_secondary_on_current_unit();
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

/* Secondary effect function 0x02 in g_battle_effect_secondary_handlers: the burst of a weapon
 * hit (EFFECT_SECONDARY_ANIMATION_WEAPON_HIT). */
s32 battle_effect_weapon_hit_secondary_handler(void) {
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

    effect = g_battle_effect_current_secondary;
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
        g_battle_effect_current_secondary->allocation = state;
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
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        battle_effect_seed_camera_tracks();
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
                battle_gfx_apply_unit_palette_modulation_to_units(MAP_COLOR_MODE_RESTORE, 2,
                    g_battle_effect_current_secondary->target.record.data.unit.runtime_id, 0, 0, 0);
            }
        }
        if (state->frame < 4) {
            if (state->count_a < 8) {
                for (i = 0;
                    i < g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A].spawns_per_frame;
                    i++) {
                    for (j = 0; j < 8; j++) {
                        if (state->ids_a[j] == 0) {
                            break;
                        }
                    }
                    state->ids_a[j] = battle_effect_alloc_slot();
                    if (state->ids_a[j] == 0) {
                        break;
                    }
                    slot = &g_battle_effect_slots[state->ids_a[j]];
                    slot->life = battle_effect_spawn_particle_motion(
                        &g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A], &slot->motion);
                    battle_effect_init_trap_animation(EFFECT_PARTICLE_GROUP_WEAPON_HIT_A,
                        g_battle_effect_current_secondary->parameter + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
                    state->count_a++;
                    if (state->count_a == 8) {
                        break;
                    }
                }
            }
            if (state->frame < 4 && state->count_b < 16) {
                for (i = 0; i < g_battle_effect_trap_particle_configs[group].spawns_per_frame; i++) {
                    for (j = 0; j < 16; j++) {
                        if (state->ids_b[j] == 0) {
                            break;
                        }
                    }
                    state->ids_b[j] = battle_effect_alloc_slot();
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                    slot = &g_battle_effect_slots[state->ids_b[j]];
                    slot->life = battle_effect_spawn_particle_motion(
                        &g_battle_effect_trap_particle_configs[group], &slot->motion);
                    battle_effect_init_trap_animation(
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
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        for (i = 0; i < 8; i++) {
            id = state->ids_a[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids_a[i] = 0;
                state->count_a--;
            }
        }
        for (i = 0; i < 16; i++) {
            id = state->ids_b[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids_b[i] = 0;
                state->count_b--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        if (g_battle_effect_current_secondary->target.record.data.unit.result_animation == EFFECT_HIT_CRITICAL) {
            if (state->frame == 0) {
                zoom.vx = zoom.vy = zoom.vz = -0x80;
                battle_camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 2) {
                zoom.vx = zoom.vy = zoom.vz = 0x2e4;
                battle_camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 4) {
                zoom.vx = zoom.vy = zoom.vz = 0;
                battle_camera_execute_zoom_command(
                    EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 12, &zoom);
                offset.vx = offset.vy = offset.vz = 4;
                battle_camera_execute_position_command(
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

/* Secondary effect function 0x03 in g_battle_effect_secondary_handlers: the
 * particles of EFFECT_SECONDARY_ANIMATION_GOLEM.
 *
 * Initializing clears the counters. Executing starts this frame's share of the
 * Golem particle group with CLUT 13 of the weapons row and, like Finalizing,
 * steps the live slots under the secondary-effect inertia threshold and a gravity
 * of (0, ONE, 0), then puts back the effect file's own values. Once no slot is
 * live and the timer has passed the group's spawn_start_frame, finish_timer
 * counts up; the handler returns 0 on the frame after that begins, and 1 until
 * then. */
s32 battle_effect_golem_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6];
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_battle_effect_current_secondary;
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
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_GOLEM, 13);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_GOLEM);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_battle_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_GOLEM].spawn_start_frame) {
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
s32 battle_effect_spell_charge_secondary_handler(void) {
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

    effect = g_battle_effect_current_secondary;
    state = &g_battle_effect_secondary_state.charge;
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        index = g_battle_effect_secondary_animations[effect->animation_id].parameter;
        g_battle_effect_current_secondary->parameter = battle_effect_get_element_index(effect->parameter);
        g_battle_effect_secondary_state.charge.speed = g_battle_effect_charge_line_params[index].speed;
        g_battle_effect_secondary_state.charge.spread_range = g_battle_effect_charge_line_params[index].spread_range;
        g_battle_effect_secondary_state.charge.spread_offset = g_battle_effect_charge_line_params[index].spread_offset;
        g_battle_effect_secondary_state.charge.duration = g_battle_effect_charge_line_params[index].duration;
        g_battle_effect_secondary_state.charge.radius = g_battle_effect_charge_line_params[index].radius;
        g_battle_effect_secondary_state.charge.spawn_interval
            = g_battle_effect_charge_line_params[index].spawn_interval;
        g_battle_effect_secondary_state.charge.color = g_battle_effect_current_secondary->parameter;
        g_battle_effect_secondary_state.charge.max_trails = g_battle_effect_charge_line_params[index].max_trails;
        g_battle_effect_secondary_state.charge.fade_row = g_battle_effect_charge_line_params[index].fade_row;
        SetDrawMode(&state->mode[0], 1, 1, PSYQ_GPU_TPAGE_ABR_ADD, 0);
        SetDrawMode(&state->mode[1], 1, 1, PSYQ_GPU_TPAGE_ABR_ADD, 0);
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
        screen = battle_unit_get_world_coords_by_runtime_id(
            g_battle_effect_current_secondary->caster.data.unit.runtime_id);
        i = battle_gfx_get_unit_spritesheet_height_by_runtime_id(
                g_battle_effect_current_secondary->caster.data.unit.runtime_id)
            + 8;
        g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].position.y
            = g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE_13].position.y = -i;
        in.vx = screen->vx;
        in.vz = screen->vz;
        in.vy = screen->vy - i;
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        RotTrans(&in, &out, &out.pad);
        state->angle = 0;
        state->done = 0;
        state->x = out.vx;
        state->y = out.vy;
        suzuki_sound_play_sfx(MAIN_SFX_SPELL_CHARGE);
        result = 1;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        if (g_battle_effect_secondary_state.charge.done == 0
            && g_battle_effect_secondary_state.charge.count.trails < g_battle_effect_secondary_state.charge.max_trails
            && rand() % g_battle_effect_secondary_state.charge.spawn_interval == 0) {
            for (i = 0; i < 16; i++) {
                if (state->trails[i].slot_id == 0) {
                    j = state->trails[i].slot_id = battle_effect_alloc_slot();
                    if (j == 0) {
                        break;
                    }
                    state->count.trails++;
                    k = (rand() & 0x1ff) + state->angle;
                    state->angle += 0x571;
                    g_battle_effect_slots[j].motion.position.x = rcos(k) * state->radius + (state->x * ONE);
                    g_battle_effect_slots[j].motion.position.y = rsin(k) * state->radius + (state->y * ONE);
                    k = k + rand() % state->spread_range - state->spread_offset - 0x800;
                    g_battle_effect_slots[j].motion.velocity.x = rcos(k) * state->speed;
                    g_battle_effect_slots[j].motion.velocity.y = rsin(k) * state->speed;
                    g_battle_effect_slots[j].life = 1;
                    for (k = 0; k < 7; k++) {
                        state->trails[i].points[k].vx = g_battle_effect_slots[j].motion.position.x >> 12;
                        state->trails[i].points[k].vy = g_battle_effect_slots[j].motion.position.y >> 12;
                    }
                    state->trails[i].age = 0;
                    break;
                }
            }
        }
        if (state->count.particles < 14) {
            for (i = 0; i < g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].spawns_per_frame;
                i++) {
                for (j = 0; j < 14; j++) {
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                }
                state->ids_b[j] = battle_effect_alloc_slot();
                if (state->ids_b[j] == 0) {
                    break;
                }
                slot = &g_battle_effect_slots[state->ids_b[j]];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE], &slot->motion);
                battle_effect_init_trap_animation(
                    EFFECT_PARTICLE_GROUP_SPELL_CHARGE, MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 15), slot);
                state->count.particles++;
                if (state->count.particles == 14) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        for (i = 0; i < 2; i++) {
            j = state->ids_a[i];
            if (j != 0 && battle_effect_update_slot(j) == 0) {
                battle_effect_free_slot(j);
                state->ids_a[i] = 0;
                state->count.auxiliary_particles--;
            }
        }
        for (i = 0; i < 14; i++) {
            j = state->ids_b[i];
            if (j != 0 && battle_effect_update_slot(j) == 0) {
                battle_effect_free_slot(j);
                state->ids_b[i] = 0;
                state->count.particles--;
            }
        }
        red = g_battle_effect_trap_colors[state->color].r;
        green = g_battle_effect_trap_colors[state->color].g;
        blue = g_battle_effect_trap_colors[state->color].b;
        offsets = &g_battle_effect_charge_line_direction_offsets[0].vx;
        offsets_y = &g_battle_effect_charge_line_direction_offsets[0].vy;
        for (i = 0; i < 16; i++) {
            j = state->trails[i].slot_id;
            if (j != 0) {
                work = &g_battle_effect_slots[j];
                work->motion.position.x = x
                    = g_battle_effect_slots[j].motion.position.x + g_battle_effect_slots[j].motion.velocity.x;
                work->motion.position.y = y
                    = g_battle_effect_slots[j].motion.position.y + g_battle_effect_slots[j].motion.velocity.y;
                age = ++state->trails[i].age;
                head = age % 7;
                if (age <= state->duration) {
                    state->trails[i].points[head].vx
                        = battle_effect_interpolate_s32_cos(x >> 12, state->x, state->duration, age);
                    fade = 6;
                    state->trails[i].points[head].vy
                        = battle_effect_interpolate_s32_cos(y >> 12, state->y, state->duration, age);
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
                    line = &state->lines[g_battle_effect_buffer_index][i * 6 + k];
                    line->x0 = state->trails[i].points[head].vx;
                    line->y0 = state->trails[i].points[head].vy;
                    setRGB0(line,
                        red
                                * g_battle_effect_charge_line_fade_levels[state->fade_row
                                        * EFFECT_CHARGE_LINE_FADE_STEPS
                                    + fade]
                            >> 8,
                        green
                                * g_battle_effect_charge_line_fade_levels[state->fade_row
                                        * EFFECT_CHARGE_LINE_FADE_STEPS
                                    + fade]
                            >> 8,
                        blue
                                * g_battle_effect_charge_line_fade_levels[state->fade_row
                                        * EFFECT_CHARGE_LINE_FADE_STEPS
                                    + fade]
                            >> 8);
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
                    setRGB1(line,
                        red
                                * g_battle_effect_charge_line_fade_levels[state->fade_row
                                        * EFFECT_CHARGE_LINE_FADE_STEPS
                                    + fade]
                            >> 8,
                        green
                                * g_battle_effect_charge_line_fade_levels[state->fade_row
                                        * EFFECT_CHARGE_LINE_FADE_STEPS
                                    + fade]
                            >> 8,
                        blue
                                * g_battle_effect_charge_line_fade_levels[state->fade_row
                                        * EFFECT_CHARGE_LINE_FADE_STEPS
                                    + fade]
                            >> 8);
                    if (LINE_G2_XY_WORD(line, 0) != LINE_G2_XY_WORD(line, 1)) {
                        dir = (ratan2(line->y1 - line->y0, line->x1 - line->x0) + 0x900) / 512;
                        line->x1 += offsets[dir * 2];
                        line->y1 += offsets_y[dir * 2];
                        ot = g_battle_effect_polygon_depth_data;
                        link = (u32)line & 0xffffff;
                        setaddr(line, getaddr(ot + 1));
                        ot[1] = (ot[1] & 0xff000000) | link;
                    }
                }
                if (state->trails[i].age >= state->duration + 7) {
                    battle_effect_free_slot(j);
                    state->trails[i].slot_id = 0;
                    state->count.trails--;
                }
            }
        }
        AddPrim(g_battle_effect_polygon_depth_data + 1, &state->mode[g_battle_effect_buffer_index]);
        if (g_battle_effect_current_secondary->phase == EFFECT_SECONDARY_PHASE_EXECUTING
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
s32 battle_effect_null_secondary_handler(void) {
    s32 result;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_battle_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x06 (Red Orbs) in g_battle_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 8, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 battle_effect_red_orbs_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_battle_effect_current_secondary;
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
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_RED_ORBS, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_RED_ORBS);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_battle_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer
                > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_RED_ORBS].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x07 in g_battle_effect_secondary_handlers: an empty
 * handler. It sets no result, although battle_effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_effect_unused_07_secondary_handler(void) {
}

/*
 * Secondary effect function 0x08 (the dust of a fall) in g_battle_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 battle_effect_fall_dust_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_battle_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_battle_effect_current_secondary->active_count = 0;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->finish_timer = 0;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_FALL_DUST, 9);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_FALL_DUST);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_battle_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer
                > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_FALL_DUST].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x09 (Splash) in g_battle_effect_secondary_handlers.
 * It shares the fall dust
 * handler's shape; each executing frame first lifts group 7 to the water
 * surface of the target's tile, 12 units per height and depth step above the
 * target's screen z. Returns 1 to keep animating.
 */
s32 battle_effect_splash_secondary_handler(void) {
    effect_secondary_data_t* effect;
    map_tile_t* tile;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_battle_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_battle_effect_current_secondary->active_count = 0;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->finish_timer = 0;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        tile = battle_map_get_unit_tile_from_world_coords(
            g_battle_effect_current_secondary->target.record.data.unit.runtime_id);
        g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].position.y
            = -((tile->height + tile->depth_half_height.depth) * 12)
            - battle_unit_get_world_coords_by_runtime_id(
                g_battle_effect_current_secondary->target.record.data.unit.runtime_id)
                  ->vy;
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_SPLASH, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_SPLASH);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_battle_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0a in g_battle_effect_secondary_handlers: an empty
 * handler. It sets no result, although battle_effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_effect_unused_0a_secondary_handler(void) {
}

/* Secondary effect function 0x0b in g_battle_effect_secondary_handlers: an empty
 * handler. It sets no result, although battle_effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_effect_unused_0b_secondary_handler(void) {
}

/*
 * Secondary effect function 0x0c (Venom Trap) in g_battle_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 0x22, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 battle_effect_venom_trap_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_battle_effect_current_secondary;
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
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_VENOM_TRAP, 12);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_VENOM_TRAP);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_battle_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer
                > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_VENOM_TRAP].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x0d (Level Up) in g_battle_effect_secondary_handlers.
 * It shares the venom trap
 * handler's shape but seeds group 5's position_y with -8 and lowers it by 3 per
 * executing frame instead of playing a sound. Returns 1 to keep animating.
 */
s32 battle_effect_level_up_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_battle_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y = -8;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_LEVEL_UP, 11);
        {
            /* Addressing through a pointer keeps the target lui/addiu pair. */
            s16* rise = &g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y;
            *rise -= 3;
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_LEVEL_UP);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_battle_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer
                > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0e in g_battle_effect_secondary_handlers: an empty
 * handler. It sets no result, although battle_effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_effect_unused_0e_secondary_handler(void) {
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
s32 battle_effect_update_death_poof_secondary(s32 unused_arg0, s32 passthrough) {
    effect_secondary_data_t* effect;
    effect_death_poof_state_t* state;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s32 spawned;
    s32 i;

    effect = g_battle_effect_current_secondary;
    result = passthrough;
    state = &g_battle_effect_death_poof_states[effect->own_slot_id];
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING: {
        state->count = 0;
        for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
            state->ids[i] = 0;
        }
        state->finish_timer = 0;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    }
    case EFFECT_SECONDARY_PHASE_EXECUTING: {
        effect_slot_t* slot;
        s32 i;
        u8 id;

        if (effect->timer >= g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame
            && effect->timer
                < g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = battle_effect_alloc_slot();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_battle_effect_slots[state->ids[i]];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF], &slot->motion);
                battle_effect_init_trap_animation(EFFECT_PARTICLE_GROUP_DEATH_POOF, EFFECT_TRAP_CLUT, slot);
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

        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
            id = state->ids[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids[i] = 0;
                state->count--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_battle_effect_current_secondary;
        current->timer = current->timer + 1;
        result = state->finish_timer != 1;
        if (state->count == 0
            && current->timer
                > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame) {
            state->finish_timer = state->finish_timer + 1;
        }
        break;
    }
    }
    return result;
}

/* Secondary effect function 0x10 in g_battle_effect_secondary_handlers: an empty
 * handler. It sets no result, although battle_effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_effect_unused_10_secondary_handler(void) {
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
s32 battle_effect_zodiac_poof_secondary_handler(void) {
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

    effect = g_battle_effect_current_secondary;
    state = &g_battle_effect_secondary_state.zodiac_poof;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->count = 0;
        for (spawned = 0; spawned < EFFECT_TRAP3_SLOT_COUNT; spawned++) {
            state->ids[spawned] = 0;
        }
        state->done = 0;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        clut_index = g_battle_effect_zodiac_poof_animations[effect->parameter];
        if (effect->timer >= g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame
            && effect->timer
                < g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_TRAP3_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = battle_effect_alloc_slot();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_battle_effect_slots[state->ids[i]];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF], &slot->motion);
                battle_effect_init_trap_animation(
                    EFFECT_PARTICLE_GROUP_ZODIAC_POOF, clut_index + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
                state->count++;
                if (state->count == EFFECT_TRAP3_SLOT_COUNT) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        for (spawned = 0; spawned < EFFECT_TRAP3_SLOT_COUNT; spawned++) {
            id = state->ids[spawned];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids[spawned] = 0;
                state->count--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        {
            effect_secondary_data_t* current_effect;

            current_effect = g_battle_effect_current_secondary;
            timer = current_effect->timer + 1;
            current_effect->timer = timer;
        }
        done = state->done;
        result = done != 1;
        if (state->count == 0
            && timer > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame) {
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
 * to battle_effect_draw_projectile_model; phase 3 finishes. The wipe indices are assigned inside the
 * subscript, each to its own variable: a separate statement or a folded
 * `timer - 1 - span` changes the order and association of the index
 * arithmetic. spin_timer keeps the timer load at full width (lh). */
s32 battle_effect_reflect_secondary_handler(void) {
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

    span = g_battle_effect_current_secondary->parameter;
    if (span < 0x1a) {
        span = 0x1a;
    }
    switch (g_battle_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        if (g_battle_effect_current_secondary->allocation != 0) {
            main_heap_free(g_battle_effect_current_secondary->allocation);
        }
        work = main_heap_alloc(0x798);
        g_battle_effect_current_secondary->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 24; j++) {
                SetPolyGT3(&work->prims[i][j]);
                SetSemiTrans(&work->prims[i][j], 1);
            }
        }
        /* Separate channel pointers retain the target's three stride bases. */
        for (i = 0; i < 2; i++) {
            colors = g_battle_effect_reflect_palette_colors;
            red = &colors->r;
            green = &colors->g;
            blue = &colors->b;
            for (j = 0; j < 6; j++) {
                work->clut[i][j] = red[j * sizeof(*colors)] | (green[j * sizeof(*colors)] << 5)
                    | (blue[j * sizeof(*colors)] << 10) | 0x8000;
            }
        }
        result = 1;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        work = g_battle_effect_current_secondary->allocation;
        timer = g_battle_effect_current_secondary->timer;
        if (span < timer && timer < span + 4) {
            work->clut[g_battle_effect_buffer_index][((lit = timer - 1) - span) * 2]
                = work->clut[g_battle_effect_buffer_index][(lit - span) * 2 + 1] = 0xffff;
        }
        if (span + 1 < g_battle_effect_current_secondary->timer) {
            work->clut[g_battle_effect_buffer_index][((cleared = g_battle_effect_current_secondary->timer - 2) - span)
                * 2] = work->clut[g_battle_effect_buffer_index][(cleared - span) * 2 + 1] = 0;
        }
        rect.x = (g_battle_effect_current_secondary->own_slot_id - 1) * 16 + 1;
        rect.y = 0x1fc;
        rect.w = 6;
        rect.h = 1;
        LoadImage(&rect, (u32*)work->clut[g_battle_effect_buffer_index]);
        caster = battle_unit_get_world_coords_by_runtime_id(
            g_battle_effect_current_secondary->caster.data.unit.runtime_id);
        target = battle_unit_get_world_coords_by_runtime_id(
            g_battle_effect_current_secondary->target.record.data.unit.runtime_id);
        setVector(&delta, caster->vx - target->vx, caster->vy - target->vy, caster->vz - target->vz);
        position.vx = target->vx;
        position.vz = target->vz;
        position.vy = target->vy
            - (s32)battle_gfx_get_unit_spritesheet_height_by_runtime_id(
                  g_battle_effect_current_secondary->target.record.data.unit.runtime_id)
                / 2;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;
        scale.vz = 0x140;
        scale.vy = 0x140;
        scale.vx = 0x140;
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
        spin.vz = 0;
        spin.vx = 0;
        spin_timer = g_battle_effect_current_secondary->timer;
        spin.vz = spin_timer << 7;
        battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        effect = g_battle_effect_current_secondary;
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
 * Secondary effect function 0x13 (Teleport) in g_battle_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 battle_effect_teleport_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_battle_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_battle_effect_current_secondary->active_count = 0;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->finish_timer = 0;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_TELEPORT, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_TELEPORT);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_battle_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer
                > g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_TELEPORT].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x14 in g_battle_effect_secondary_handlers: a
 * projectile flying in a straight line. It runs the crossbow bolt (the arrow
 * model), the thrown stone and the thrown item animations.
 *
 * Initializing measures the trajectory, takes the flight time from
 * g_battle_effect_trajectory_step_count and allocates what the projectile draws
 * with: the polygon buffer of the arrow or stone model, or for a thrown item the
 * shuriken model's buffer (items ITEM_ID_SHURIKEN to ITEM_ID_YAGYU_DARKNESS) or
 * else an effect slot holding the item's icon as a quad. Executing moves 0x8000
 * (8 units in Q12) along the line each frame, aims the model along it and draws
 * it: the arrow at scale 0x40, the stone and the shuriken at 0xc0 and spinning by
 * the frames left, any other item as its icon sprite.
 *
 * With two frames left and a unit hit by the trajectory
 * (g_battle_effect_trajectory_hit_unit_id), the target's hit reaction
 * (battle_unit_call_bow_hardcoding_by_runtime_id) starts, unless the item is one of
 * the balls or a consumable (ITEM_ID_POTION to ITEM_ID_PHOENIX_DOWN). When the
 * count runs out the icon slot is freed; a unit target whose result is neither
 * MISSED nor NO_EFFECT releases the requested ability effect at animation speed
 * 2, and otherwise a requested effect is dropped after the hit reaction. The
 * handler then enters Finalizing, whose return of 0 removes it.
 *
 * g_battle_effect_projectile_spin_passes is the count of frames left. result is
 * unset for a phase outside 1-3, and scale and spin for a model other than the
 * arrow or the stone. */
s32 battle_effect_linear_projectile_secondary_handler(void) {
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

    switch (g_battle_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        setVector(&delta, g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx,
            g_battle_effect_trajectory_destination.vy - g_battle_effect_trajectory_origin.vy,
            g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz);
        g_battle_effect_projectile_length
            = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
        g_battle_effect_projectile_delta = delta;
        g_battle_effect_projectile_progress = 0;
        g_battle_effect_projectile_speed = 0x8000;
        g_battle_effect_projectile_spin_passes = g_battle_effect_trajectory_step_count;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        if (g_battle_effect_current_secondary->allocation != 0) {
            main_heap_free(g_battle_effect_current_secondary->allocation);
        }
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
        if ((parameter & 0xff) == EFFECT_MODEL_ARROW) {
            arrow = main_heap_alloc(0xc88);
            g_battle_effect_current_secondary->allocation = arrow;
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
            g_battle_effect_current_secondary->allocation = work12;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 12; j++) {
                    SetPolyG3(&work12->g3[i][j]);
                }
            }
        }
        if (parameter == EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            item = g_battle_effect_current_secondary->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                work16 = main_heap_alloc(0x380);
                g_battle_effect_current_secondary->allocation = work16;
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 16; j++) {
                        SetPolyG3(&work16->g3[i][j]);
                    }
                }
            } else {
                gfx_set_item_icon_sprite(&sprite, item);
                g_battle_effect_current_secondary->slot_ids[0] = battle_effect_alloc_slot();
                slot = &g_battle_effect_slots[g_battle_effect_current_secondary->slot_ids[0]];
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
        g_battle_effect_projectile_progress += g_battle_effect_projectile_speed;
        battle_effect_find_projectile_xyz_along_trajectory(&g_battle_effect_projectile_delta,
            g_battle_effect_projectile_length, g_battle_effect_projectile_progress, &position);
        setVector(&position, g_battle_effect_trajectory_origin.vx + position.vx,
            g_battle_effect_trajectory_origin.vy + position.vy, g_battle_effect_trajectory_origin.vz + position.vz);
        delta.vx = g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx;
        delta.vz = g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz;
        delta.vy = g_battle_effect_trajectory_destination.vy - g_battle_effect_trajectory_origin.vy;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
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
                setVector(&spin, 0, g_battle_effect_projectile_spin_passes * 256,
                    g_battle_effect_projectile_spin_passes * 128);
            }
            battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        } else {
            item = g_battle_effect_current_secondary->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                spin.vy = 0;
                spin.vx = 0;
                spin.vz = -(g_battle_effect_projectile_spin_passes * 256);
                battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, EFFECT_MODEL_SHURIKEN);
            } else {
                battle_effect_draw_item_drop_sprite(&position, item, 0);
            }
        }
        if (g_battle_effect_projectile_spin_passes == 2 && g_battle_effect_trajectory_hit_unit_id != -1) {
            kind = g_battle_effect_current_secondary->used_weapon_id;
            if ((kind < ITEM_ID_FIRE_BALL || kind > ITEM_ID_LIGHTNING_BALL)
                && (kind < ITEM_ID_POTION || kind > ITEM_ID_PHOENIX_DOWN)
                && g_battle_effect_current_secondary->target_count != 0) {
                battle_unit_call_bow_hardcoding_by_runtime_id(
                    g_battle_effect_current_secondary->caster.data.unit.runtime_id,
                    g_battle_effect_current_secondary->target.record.data.unit.runtime_id);
            }
        }
        if (g_battle_effect_projectile_spin_passes > 0) {
            g_battle_effect_projectile_spin_passes--;
        }
        if (g_battle_effect_projectile_spin_passes == 0) {
            if (g_battle_effect_current_secondary->slot_ids[0] != 0) {
                battle_effect_free_slot(g_battle_effect_current_secondary->slot_ids[0]);
                g_battle_effect_current_secondary->slot_ids[0] = 0;
            }
            if (g_battle_effect_current_secondary->target.record.target_type == EFFECT_TARGET_TYPE_UNIT
                && g_battle_effect_current_secondary->target.record.data.unit.result_animation != EFFECT_HIT_MISSED
                && g_battle_effect_current_secondary->target.record.data.unit.result_animation
                    != EFFECT_HIT_NO_EFFECT) {
                battle_effect_play();
                battle_state_set_animation_speed(2);
            } else if (battle_effect_get_playback_phase() == EFFECT_PLAYBACK_PHASE_REQUESTED) {
                if (g_battle_effect_current_secondary->target_count != 0) {
                    battle_unit_call_bow_hardcoding_by_runtime_id(
                        g_battle_effect_current_secondary->caster.data.unit.runtime_id,
                        g_battle_effect_current_secondary->target.record.data.unit.runtime_id);
                }
                battle_effect_cancel_playback_request();
            }
            g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
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
 * Secondary effect function 0x15 (Equip Break) in g_battle_effect_secondary_handlers.
 * Same shape as the venom trap
 * handler, but its particle group is held in a variable, so its spawn_start_frame lookup
 * is an indexed table read. Returns 1 to keep animating.
 */
s32 battle_effect_equip_break_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    vec3s32_t gravity;
    s32 group = EFFECT_PARTICLE_GROUP_EQUIP_BREAK;
    s32 result;
    s16 finish_timer;

    effect = g_battle_effect_current_secondary;
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
        battle_effect_start_group(group, 10);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(group);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_battle_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_battle_effect_trap_particle_configs[group].spawn_start_frame) {
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
s32 battle_effect_summon_charge_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_ring_state_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x68-byte frame. */
    s32 result;
    s32 i;
    s32 j;
    s32 k;
    u8 id;

    effect = g_battle_effect_current_secondary;
    state = &g_battle_effect_secondary_state.summon_ring;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->index = 0;
        state->radius = 0;
        state->angle_step = 0;
        state->angle = 0;
        battle_effect_resolve_target_world_coords(
            0, EFFECT_TARGET_INDEX_CASTER, &g_battle_effect_context, &state->center);
        state->brightness = 0;
        state->center.vy -= 24;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                state->points[i][j].vx = state->points[i][j].vy = state->points[i][j].vz = 0;
            }
        }
        for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
            j = g_battle_effect_current_secondary->slot_ids[i] = battle_effect_alloc_slot();
            if (j != 0) {
                slot = &g_battle_effect_slots[j];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_battle_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SUMMON_CHARGE], &slot->motion);
                battle_effect_init_trap_animation(EFFECT_PARTICLE_GROUP_SUMMON_CHARGE, EFFECT_RING_CLUT, slot);
            }
        }
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->finish_timer = 0;
        g_battle_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
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
                id = g_battle_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_battle_effect_slots[id];
                    slot->color.r = slot->color.g = slot->color.b
                        = g_battle_effect_summon_ring_brightness[j] * state->brightness / 128;
                }
            }
        }
        for (i = 0; i < 3; i++) {
            k = state->index;
            for (j = 0; j < 10; j++) {
                id = g_battle_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_battle_effect_slots[id];
                    slot->motion.position.x = (state->center.vx + state->points[i][k].vx) * ONE;
                    slot->motion.position.y = (state->center.vy + state->points[i][k].vy) * ONE;
                    slot->motion.position.z = (state->center.vz + state->points[i][k].vz) * ONE;
                    battle_effect_update_slot(g_battle_effect_current_secondary->slot_ids[i * 10 + j]);
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
        if (g_battle_effect_current_secondary->finish_timer >= 61) {
            for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
                if (g_battle_effect_current_secondary->slot_ids[i] != 0) {
                    battle_effect_free_slot(g_battle_effect_current_secondary->slot_ids[i]);
                    g_battle_effect_current_secondary->slot_ids[i] = 0;
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
s32 battle_effect_update_secondary_effects(void) {
    effect_secondary_data_t* effect;
    u32 previous_slot_id;

    g_battle_effect_polygon_depth_data = main_gfx_get_otag();
    g_battle_effect_buffer_index = 1 - g_battle_effect_buffer_index;
    g_battle_effect_current_slot_id = g_battle_effect_secondary_active_tail;
    if (g_battle_effect_secondary_active_tail == 0) {
        return 0;
    }

    do {
        effect = &g_battle_effect_secondary_data[g_battle_effect_current_slot_id];
        g_battle_effect_current_secondary = effect;
        previous_slot_id = effect->previous_slot_id;
        if (g_battle_effect_secondary_handlers[effect->function_id]() == 0) {
            battle_effect_remove_secondary(g_battle_effect_current_slot_id);
        }
        g_battle_effect_current_slot_id = previous_slot_id;
    } while (previous_slot_id != 0);

    return 1;
}
