#include "fft/battle.h"
#include "psx/types.h"

/* Timeline track 3 handler: sets the effect camera zoom target and duration.
 *
 * Mode bits 0x1e0 pick the source: the given vector, the current camera zoom
 * (0xc0) or the saved zoom at g_battle_effect_camera_zoom_saved (0x100), the latter two offset by
 * zoom when present. Other modes leave the camera untouched. Bits 0x1e00 are
 * kept in g_battle_effect_camera_zoom_mode. */
void battle_camera_execute_zoom_command(s32 flags, s32 unused, s32 duration, VECTOR* zoom) {
    switch (flags & 0x1e0) {
    case 0x0:
    case 0x40:
    case 0x80:
    case 0x140:
    case 0x180:
        g_battle_effect_camera_zoom_target = *zoom;
        break;
    case 0xc0:
        g_battle_effect_camera_zoom_target = *battle_camera_get_zoom();
        if (zoom) {
            battle_effect_add_vectors(
                &zoom->vx, &g_battle_effect_camera_zoom_target.vx, &g_battle_effect_camera_zoom_target.vx);
        }
        break;
    case 0x100:
        g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_saved;
        if (zoom) {
            battle_effect_add_vectors(
                &zoom->vx, &g_battle_effect_camera_zoom_target.vx, &g_battle_effect_camera_zoom_target.vx);
        }
        break;
    default:
        return;
    }
    g_battle_effect_camera_zoom_mode = flags & 0x1e00;
    g_battle_effect_camera_zoom_duration = duration;
    g_battle_effect_camera_zoom_frame = 0;
}
