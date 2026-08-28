#include "fft/battle.h"
#include "fft/event.h"
#include "psx/types.h"

/* Battle twin of world_script_reset_event_state. Where WORLD has no-op stubs, BATTLE
 * snapshots the camera into script variables and resets the script camera
 * transform while no thread occupies slot 1. */
void battle_script_reset_event_state(void) {
    battle_unit_reset_animation_states();
    if (battle_thread_is_running_8014cc94(1) == 0) {
        battle_camera_store_state_to_script_variables();
        battle_camera_reset_script_transform();
        g_battle_camera_speed_curve = 0;
        g_battle_camera_position_locked = 0;
    }
}
