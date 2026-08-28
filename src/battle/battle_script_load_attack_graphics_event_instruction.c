#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Enter ATTACK.OUT when script variable 0x1fc is clear: mark the transition,
 * load companion executable 0xd, then hand off to its scenario-music,
 * formation-sprite, and portrait entry points. */
void battle_script_load_attack_graphics_event_instruction(void) {
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
        g_battle_script_attack_entry_mode = 2;
        battle_menu_request_open_companion_executable(0xD);
        battle_thread_yield();
        attack_load_scenario_conditionals();
        attack_gfx_build_formation_sprites();
        attack_gfx_load_portraits();
    }
}
