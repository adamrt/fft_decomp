#include "fft/battle_map.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Stop the weather sound effects and every color rotation the 32 map texture
 * animations hold a handle for. Always returns 0. */
s32 map_stop_clut_cycles_and_weather_sfx(void) {
    s32 index;

    sound_stop_weather_sfx();

    for (index = 0; index < MAP_TEXTURE_ANIMATION_COUNT; index++) {
        if (g_map_texture_animation_cycle_handles[index] != 0) {
            map_stop_clut_cycle(g_map_texture_animation_cycle_handles[index]);
            g_map_texture_animation_cycle_handles[index] = 0;
        }
    }

    g_sound_weather_sfx_enabled = 0;
    return 0;
}
