#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "psx/types.h"

/* Runs one step of the battle map initialization sequence and returns the
 * next step; steps that poll a loader stay on the same step until it
 * finishes. `unused_18` reproduces the target's unreferenced frame slot at sp+0x18.
 *
 * Jump table: .rodata 0x80068260, 13 32-bit addresses. */
s32 map_step_init_sequence(s32 map_id, s32 step) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* No sound id is loaded: the callee receives map_id, still in $a0. */
        main_sound_stop_sfx(map_id);
        step++;
        camera_init_defaults();
        map_init_palette_state();
        map_load_palette_data(&g_map_weather_particle_palette, MAP_CLUT_ROW_SPRITES, 0, 0);
        g_main_gfx_unit_graphics_load_queue.count = 0;
        menu_request_open_small();
        break;
    case 1:
        if (battle_menu_poll_small_load() == 0) {
            step++;
        }
        break;
    case 2:
        if (map_load_data(map_id, MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (map_load_data(map_id, MAP_LOAD_COMMAND_POLL) == 0) {
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
        battle_gfx_clear_unit_graphics_load_state(map_id);
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
        battle_unit_build_deployed_units_data(g_main_deployed_unit_map_coordinates, 0);
        step++;
        break;
    case 9:
    case 10:
        step++;
        break;
    case 11:
        if (g_main_system_frontend_world_result == 1
            || script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            main_sound_unload_current_scenario_music();
            /* Called with no argument: the sound id is whatever
             * main_sound_unload_current_scenario_music left in $a0. */
            ((void (*)(void))main_sound_stop_sfx)();
            main_sound_set_current_music_track(g_battle_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        sound_start_weather_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        map_init_background_gradient();
        map_init_ambient_light();
        map_init_darkness();
        map_upload_clut_rows();
        action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RESET);
        ai_init_workspace();
        step++;
        break;
    }
    return step;
}
