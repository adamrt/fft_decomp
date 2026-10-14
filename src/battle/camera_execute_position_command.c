#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

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
