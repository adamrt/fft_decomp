#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Reset the unit animation states and, when the current-event variable is set,
 * fade the screen color modulation to zero and start the event thread (slot 1)
 * on script_run_next_event.
 *
 * While the battle has not started it first stores the camera in the script
 * variables and resets the script camera transform. Where WORLD's copy calls two
 * empty functions, BATTLE does that camera work. */
void script_start_current_event_thread(void) {
    unit_reset_animation_states();
    g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_NORMAL;
    if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        camera_store_state_to_script_variables();
        map_reset_script_light_transform();
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
        battle_gfx_start_screen_color_modulation_fade(2, 0, 0, 0, 2);
        thread_start(THREAD_SLOT_EVENT, script_run_next_event);
    }
}
