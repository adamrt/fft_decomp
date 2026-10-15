#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Reset unit animation state; while the event slot is idle, run the scene
 * camera/light reset hooks and clear camera interpolation and position lock. */
void script_reset_event_state(void) {
    unit_reset_animation_states();
    if (thread_is_running_asm_variant(THREAD_SLOT_EVENT) == 0) {
        camera_store_state_to_script_variables_variant();
        map_reset_script_light_transform_variant();
        g_camera_speed_curve = 0;
        g_camera_position_locked = 0;
    }
}
