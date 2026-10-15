#include "fft/battle_camera.h"

/* Let the held pad directions pan the camera across the map
 * (camera_update_map_pan); the camera stops following the cursor tile. */
void camera_enable_map_pan(void) {
    g_camera_map_pan_enabled = 1;
}
