#include "fft/battle.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "fft/script_variables.h"
#include "psx/types.h"

/* Run a weather display command.
 *
 * Command 0x8b reads the weather from script variable 0x23 and re-issues the
 * set-up commands for values 0-1, 2, 3 and 4, choosing each variant by the
 * snow bit of the map weather flags. The weather local is an s8: compares on
 * the promoted value copy it into a scratch register for the 3 and 4 tests,
 * as the target does. */
void battle_map_set_weather_texture_overlay(s32 command) {
    s8 weather;

    switch (command & 0xff) {
    case 0x8c:
        battle_map_light_state_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
        SetColorMatrix(g_battle_map_lightning_state.color_matrices);
        SetBackColor(g_battle_map_back_color_red, g_battle_map_back_color_green, g_battle_map_back_color_blue);
        break;
    case 0x8b:
        weather = battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        g_battle_map_lightning_mode = 0;
        weather &= 7;
        if (weather < 2) {
            battle_sound_stop_weather_sfx();
            battle_map_set_weather_texture_overlay(0x54);
            battle_map_set_weather_texture_overlay(0x8c);
        }
        if (weather == BATTLE_WEATHER_RAIN) {
            if (g_battle_map_weather_flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_set_weather_texture_overlay(0x7e);
                battle_map_set_weather_texture_overlay(0x8c);
            } else {
                battle_sound_stop_weather_sfx();
                battle_map_set_weather_texture_overlay(0x7c);
                battle_map_set_weather_texture_overlay(0x8c);
            }
        }
        if (weather == BATTLE_WEATHER_STORM) {
            if (g_battle_map_weather_flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_set_weather_texture_overlay(0x53);
                battle_map_set_weather_texture_overlay(0x8c);
            } else {
                g_battle_sound_primary_weather_sfx_id = 0x10002;
                if (g_battle_sound_weather_sfx_enabled != 0) {
                    main_sound_play_weather_sfx(0x10002);
                }
                battle_map_set_weather_texture_overlay(0x55);
                battle_map_set_weather_texture_overlay(0x8c);
            }
        }
        if (weather == BATTLE_WEATHER_STRONG_STORM) {
            if (g_battle_map_weather_flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_set_weather_texture_overlay(0x7f);
                battle_map_set_weather_texture_overlay(0x8c);
            } else {
                battle_sound_stop_weather_sfx();
                battle_map_set_weather_texture_overlay(0x7d);
                battle_map_set_weather_texture_overlay(0x8a);
                g_battle_sound_primary_weather_sfx_id = 0x10002;
                if (g_battle_sound_weather_sfx_enabled != 0) {
                    main_sound_play_weather_sfx(0x10002);
                }
            }
        }
        break;
    case 0x89:
        g_battle_map_lightning_mode = 2;
        g_battle_map_lightning_state.timer = 0x3c;
        g_battle_sound_primary_weather_sfx_id = 0x10002;
        if (g_battle_sound_weather_sfx_enabled != 0) {
            main_sound_play_weather_sfx(0x10002);
        }
        g_battle_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][2] = 0;
        battle_map_set_weather_texture_overlay(0x8c);
        break;
    case 0x8a:
        g_battle_map_lightning_mode = 1;
        g_battle_map_lightning_state.timer = 0x3c;
        g_battle_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][2] = 0;
        battle_map_set_weather_texture_overlay(0x8c);
        break;
    case 0x53:
        g_battle_map_weather_fall_speed = 1;
        g_battle_map_weather_primary_speed = 4;
        battle_map_init_weather_particles(0x8f);
        break;
    case 0x7e:
        g_battle_map_weather_fall_speed = 1;
        g_battle_map_weather_primary_speed = 1;
        battle_map_init_weather_particles(0x8f);
        break;
    case 0x7f:
        g_battle_map_weather_fall_speed = 3;
        g_battle_map_weather_primary_speed = 0x10;
        battle_map_init_weather_particles(0x8f);
        break;
    case 0x55:
        g_battle_map_weather_fall_speed = 5;
        g_battle_map_weather_primary_speed = 6;
        g_battle_map_rain_secondary_fall_speed = 9;
        battle_map_init_weather_particles(0x90);
        break;
    case 0x7c:
        g_battle_map_weather_fall_speed = 3;
        g_battle_map_weather_primary_speed = 5;
        g_battle_map_rain_secondary_fall_speed = 8;
        battle_map_init_weather_particles(0x90);
        break;
    case 0x7d:
        g_battle_map_weather_fall_speed = 5;
        g_battle_map_weather_primary_speed = 9;
        g_battle_map_rain_secondary_fall_speed = 0x12;
        battle_map_init_weather_particles(0x90);
        break;
    case 0x54:
        g_battle_map_weather_effect_mode = 0x54;
        g_battle_map_lightning_mode = 0;
        break;
    }
}
