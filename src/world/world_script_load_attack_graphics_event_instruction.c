#include "fft/event_attack.h"
#include "fft/world.h"
#include "psx/types.h"

/* Enter the attack overlay when script variable 0x1fc is clear: mark the
 * transition state, load BIN file 0xd, then hand off to the overlay's
 * scenario-music, formation-sprite and portrait entry points. */
void world_script_load_attack_graphics_event_instruction(void) {
    if (world_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
        g_world_script_attack_entry_mode = 2;
        world_bin_load_file(0xd);
        world_thread_yield();
        attack_load_scenario_conditionals();
        attack_gfx_build_formation_sprites();
        attack_gfx_load_portraits();
    }
}
