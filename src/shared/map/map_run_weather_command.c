#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/types.h"

/* Run a weather display command.
 *
 * Command 0x8b reads the weather from script variable 0x23 and re-issues the
 * set-up commands for values 0-1, 2, 3 and 4, choosing each variant by the
 * snow bit of the map weather flags. The weather local is an s8: compares on
 * the promoted value copy it into a scratch register for the 3 and 4 tests,
 * as the target does. */
void map_run_weather_command(map_weather_command_e command) {
    s8 weather;

    switch (command & 0xff) {
    case MAP_WEATHER_COMMAND_RESTORE_LIGHTING:
        map_light_state_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
        SetColorMatrix(g_map_lightning_state.color_matrices);
        SetBackColor(g_map_back_color_red, g_map_back_color_green, g_map_back_color_blue);
        break;
    case MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER:
        weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        g_map_lightning_mode = MAP_LIGHTNING_OFF;
        weather &= 7;
        if (weather < BATTLE_WEATHER_RAIN) {
            sound_stop_weather_sfx();
            map_run_weather_command(MAP_WEATHER_COMMAND_CLEAR);
            map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_RAIN) {
            if (g_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                sound_stop_weather_sfx();
                map_run_weather_command(MAP_WEATHER_COMMAND_LIGHT_SNOW);
            } else {
                sound_stop_weather_sfx();
                map_run_weather_command(MAP_WEATHER_COMMAND_LIGHT_RAIN);
            }
            map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_STORM) {
            if (g_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                sound_stop_weather_sfx();
                map_run_weather_command(MAP_WEATHER_COMMAND_SNOW);
            } else {
                g_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
                if (g_sound_weather_sfx_enabled != 0) {
                    main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
                }
                map_run_weather_command(MAP_WEATHER_COMMAND_RAIN);
            }
            map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_STRONG_STORM) {
            if (g_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                sound_stop_weather_sfx();
                map_run_weather_command(MAP_WEATHER_COMMAND_HEAVY_SNOW);
                map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
            } else {
                sound_stop_weather_sfx();
                map_run_weather_command(MAP_WEATHER_COMMAND_HEAVY_RAIN);
                map_run_weather_command(MAP_WEATHER_COMMAND_LIGHTNING);
                g_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
                if (g_sound_weather_sfx_enabled != 0) {
                    main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
                }
            }
        }
        break;
    case MAP_WEATHER_COMMAND_THUNDER:
        g_map_lightning_mode = MAP_LIGHTNING_ONCE;
        g_map_lightning_state.timer = 0x3c;
        g_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
        if (g_sound_weather_sfx_enabled != 0) {
            main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
        }
        g_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_map_lightning_state.color_matrices[1].m[2][2] = 0;
        map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case MAP_WEATHER_COMMAND_LIGHTNING:
        g_map_lightning_mode = MAP_LIGHTNING_RANDOM;
        g_map_lightning_state.timer = 0x3c;
        g_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_map_lightning_state.color_matrices[1].m[2][2] = 0;
        map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case MAP_WEATHER_COMMAND_SNOW:
        g_map_weather_fall_speed = 1;
        g_map_weather_primary_speed = 4;
        map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_LIGHT_SNOW:
        g_map_weather_fall_speed = 1;
        g_map_weather_primary_speed = 1;
        map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_HEAVY_SNOW:
        g_map_weather_fall_speed = 3;
        g_map_weather_primary_speed = 0x10;
        map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_RAIN:
        g_map_weather_fall_speed = 5;
        g_map_weather_primary_speed = 6;
        g_map_rain_secondary_fall_speed = 9;
        map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_LIGHT_RAIN:
        g_map_weather_fall_speed = 3;
        g_map_weather_primary_speed = 5;
        g_map_rain_secondary_fall_speed = 8;
        map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_HEAVY_RAIN:
        g_map_weather_fall_speed = 5;
        g_map_weather_primary_speed = 9;
        g_map_rain_secondary_fall_speed = 0x12;
        map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_CLEAR:
        g_map_weather_effect_mode = MAP_WEATHER_EFFECT_CLEAR;
        g_map_lightning_mode = MAP_LIGHTNING_OFF;
        break;
    }
}
