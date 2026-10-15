#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"

/* Resets the camera for a new map: screen offset (0x100, 0xa0, 0x280), zoom ONE,
 * pitch 0x12e (the low tilt), yaw 0xe00, position at the origin, cursor on tile
 * (0, 0), no acting or casting unit, no rotation, tilt or zoom in progress and
 * directional panning off. */
void camera_init_defaults(void) {
    main_util_set_vector(&g_camera_current_screen_offset, 0x100, 0xA0, 0x280);
    main_util_set_vector(&g_camera_zoom, ONE, ONE, ONE);
    main_util_set_svector(&g_camera_render_state, 0x12E, 0xE00, 0);
    main_util_set_vector(&g_camera_current_real_coords, 0, 0, 0);

    g_battle_casting_runtime_id = 0xFF;
    g_battle_unit_source_runtime_id = 0xFF;
    g_battle_cursor_y = 0;
    g_battle_cursor_z = 0;
    g_battle_cursor_x = 0;
    g_camera_rotation_action = CAMERA_ROTATION_NONE;
    g_camera_zoom_action = CAMERA_STEP_NONE;
    g_map_zoom_target = MAP_ZOOM_IN;
    g_camera_tilt_action = CAMERA_STEP_NONE;
    g_map_tilt_target = MAP_TILT_LOW;
    g_camera_map_pan_enabled = 0;
}
