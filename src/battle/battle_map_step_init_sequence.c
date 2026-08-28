#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "psx/types.h"

/* Runs one step of the battle map initialization sequence and returns the
 * next step; steps that poll a loader stay on the same step until it
 * finishes. `unused_18` reproduces the target's unreferenced frame slot at sp+0x18.
 *
 * Jump table: .rodata 0x80068260, 13 words. */
s32 battle_map_step_init_sequence(s32 map_id, s32 step) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* Called without its sound id argument. */
        ((void (*)(void))main_sound_stop_sfx)();
        step++;
        battle_camera_init_defaults();
        battle_map_init_palette_state();
        battle_map_load_palette_data(g_battle_map_weather_particle_palette, 2, 0, 0);
        g_unit_graphics_load_count = 0;
        battle_menu_request_companion_executable_8();
        break;
    case 1:
        if (battle_poll_companion_executable_request() == 0) {
            step++;
        }
        break;
    case 2:
        if (battle_map_load_data(map_id, 0x75) != 0) {
            step++;
        }
        break;
    case 3:
        if (battle_map_load_data(map_id, 0x76) == 0) {
            step++;
        }
        break;
    case 4:
        if (battle_unit_select_and_open_entd() != 0) {
            step++;
        }
        break;
    case 5:
        if (battle_unit_poll_and_load_entd_units() == 0) {
            step++;
        }
        break;
    case 6:
        battle_gfx_clear_four_state_words();
        step++;
        break;
    case 7:
        do {
            result = battle_gfx_step_queued_unit_graphics_load();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 8:
        /* The target passes a second argument (0) the one-parameter callee ignores. */
        ((void (*)(s32*, s32))battle_unit_build_deployed_units_data)(g_main_deployed_unit_map_coordinates, 0);
        step++;
        break;
    case 9:
    case 10:
        step++;
        break;
    case 11:
        if (g_main_system_frontend_world_result == 1
            || battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            main_sound_unload_current_scenario_music();
            /* Called without its sound id argument. */
            ((void (*)(void))main_sound_stop_sfx)();
            main_sound_set_current_music_track(g_battle_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        battle_sound_start_weather_sfx();
        /* The target calls these three-parameter initializers without loading arguments. */
        ((void (*)(void))battle_map_init_background_gradient)();
        ((void (*)(void))battle_map_init_ambient_light)();
        ((void (*)(void))battle_map_init_darkness)();
        battle_map_update_lighting();
        battle_action_run_between_turn_control(1);
        battle_ai_init_workspace();
        step++;
        break;
    }
    return step;
}
