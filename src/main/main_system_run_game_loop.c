#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "psx/types.h"

void main_system_run_game_loop(void) {
    int open_mode;

    g_main_system_frontend_world_result = 0;

    for (;;) {
        main_item_init_new_game_inventory();

        if (g_main_system_frontend_world_result == 5) {
            open_mode = 1;
        } else {
            main_boot_reset_game_state();
            open_mode = 0;
        }

        if (main_overlay_exec_open_bin_main_loop(open_mode) == 0) {
            g_main_system_go_straight_to_battle = 1;
            g_main_system_frontend_world_result = 0;
        } else {
            g_main_system_go_straight_to_battle = 0;
        }

        do {
            if (g_main_system_go_straight_to_battle == 0) {
                g_main_system_frontend_world_result = main_overlay_open_world_and_wldcore(1);
            }

            if (g_main_system_frontend_world_result == 5) {
                break;
            }

            main_overlay_exec_battle_bin();
            battle_state_run_game_loop();
            main_sound_stop_weather_sfx_music();
            g_main_system_go_straight_to_battle = 0;

            if (g_main_system_game_flow_state == 2) {
                main_system_stop_display_and_audio_for_game_reset();
                break;
            }
        } while (g_main_system_game_flow_state != 3);

        if (g_main_system_game_flow_state == 3) {
            main_sound_stop_weather_sfx_music();
            main_sound_unload_scenario_music_and_tunes();
            battle_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 1);
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x12A);
            main_overlay_exec_open_bin_ending();
            main_overlay_exec_battle_bin();
            main_gfx_load_zodiac_frame();
            battle_state_run_game_loop();
            main_system_stop_display_and_audio_for_game_reset();
        }
    }
}
