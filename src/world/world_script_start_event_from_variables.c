#include "fft/battle_gfx.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

/* Resets unit animation states and, when the current-event script variable is set, clears the screen
 * colour modulation and starts the next event on thread 1. */
void world_script_start_event_from_variables(void) {
    world_unit_reset_animation_states();
    g_world_script_attack_entry_mode = 1;
    if (world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) != 0) {
        world_noop_800f6ea8();
        world_noop_800f29d8();
        g_world_camera_position_locked = 0;
        g_world_camera_speed_curve = 0;
    }
    if (world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_world_camera_position_locked = 0;
        g_world_camera_speed_curve = 0;
        battle_gfx_start_screen_color_modulation_fade(2, 0, 0, 0, 2);
        world_thread_start(1, world_script_run_next_event_2);
    }
}
