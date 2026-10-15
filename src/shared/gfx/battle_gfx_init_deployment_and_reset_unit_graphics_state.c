#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/event_attack.h"
#include "fft/script.h"

/* Builds the deployed units' data when a map starts and rewinds the unit graphics
 * loader.
 *
 * A battle whose deployment screen is skipped places the units from
 * g_main_deployed_unit_map_coordinates; otherwise the coordinate slots of the
 * deployment screen are used, and in the debug battle style the red team is set
 * up from them first. */
void battle_gfx_init_deployment_and_reset_unit_graphics_state(void) {
    g_main_gfx_unit_graphics_load_queue.count = 0;
    if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) == 0) {
        if (g_battle_deployment_skipped != 0) {
            battle_unit_build_deployed_units_data(g_main_deployed_unit_map_coordinates, 0);
            g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
            g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
            g_battle_gfx_unit_graphics_load_state.index = 0;
            g_battle_gfx_unit_graphics_load_state.buffer = 0;
            return;
        }
    } else {
        battle_unit_init_deployed_units_data_for_debug_red_team(attack_deploy_get_coordinate_slots(), 0);
    }
    battle_unit_build_deployed_units_data(attack_deploy_get_coordinate_slots(), 0);
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
}
