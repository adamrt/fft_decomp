#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "psx/types.h"

void battle_gfx_init_deployment_and_reset_unit_graphics_state(void) {
    g_unit_graphics_load_count = 0;
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) == 0) {
        /* These calls pass a second argument (0) the one-parameter callees ignore. */
        if (g_battle_deployment_skipped != 0) {
            ((void (*)(s32*, s32))battle_unit_build_deployed_units_data)(g_main_deployed_unit_map_coordinates, 0);
            g_battle_gfx_state_words[0] = 0;
            g_battle_gfx_state_words[1] = 0;
            g_battle_gfx_state_words[2] = 0;
            g_battle_gfx_state_words[3] = 0;
            return;
        }
    } else {
        ((void (*)(s32*, s32))battle_unit_init_deployed_units_data_for_debug_red_team)(
            attack_deploy_get_coordinate_slots(), 0);
    }
    ((void (*)(s32*, s32))battle_unit_build_deployed_units_data)(attack_deploy_get_coordinate_slots(), 0);
    g_battle_gfx_state_words[0] = 0;
    g_battle_gfx_state_words[1] = 0;
    g_battle_gfx_state_words[2] = 0;
    g_battle_gfx_state_words[3] = 0;
}
