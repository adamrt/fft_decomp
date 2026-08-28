#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/event.h"
#include "fft/script_variables.h"
#include "psx/types.h"

extern void battle_gfx_start_screen_color_modulation_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
void battle_script_start_current_event_thread(void) {
    battle_unit_reset_animation_states();
    g_battle_script_attack_entry_mode = 1;
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) != 0) {
        battle_camera_store_state_to_script_variables();
        battle_camera_reset_script_transform();
        g_battle_camera_position_locked = 0;
        g_battle_camera_speed_curve = 0;
    }
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_battle_camera_position_locked = 0;
        g_battle_camera_speed_curve = 0;
        battle_gfx_start_screen_color_modulation_fade(2, 0, 0, 0, 2);
        battle_thread_start(1, battle_script_run_next_event);
    }
}
