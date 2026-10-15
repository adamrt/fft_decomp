#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

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
