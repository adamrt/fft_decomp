#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

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
