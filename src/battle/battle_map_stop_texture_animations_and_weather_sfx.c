#include "fft/battle.h"
#include "psx/types.h"

enum {
    MAP_TEXTURE_ANIMATION_COUNT = 32,
};

s32 battle_map_stop_texture_animations_and_weather_sfx(void) {
    s32 index;
    s32* handle;

    battle_sound_stop_weather_sfx();

    index = 0;
    handle = g_battle_map_texture_animation_handles;
    do {
        if (*handle != 0) {
            battle_map_stop_texture_animation(*handle);
            *handle = 0;
        }
        index++;
        handle++;
    } while (index < MAP_TEXTURE_ANIMATION_COUNT);

    g_battle_sound_weather_sfx_enabled = 0;
    return 0;
}
