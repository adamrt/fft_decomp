#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Stop the rain sound and the two thunder sounds and forget their ids. The result
 * is an unset local; no caller reads it. */
s32 sound_stop_weather_sfx(void) {
    s32 result;
    s32 sound_id;

    sound_id = g_sound_primary_weather_sfx_id;
    if (sound_id != 0) {
        main_sound_stop_sfx(sound_id);
        g_sound_primary_weather_sfx_id = 0;
    }

    sound_id = g_sound_secondary_weather_sfx_id;
    if (sound_id != 0) {
        main_sound_stop_sfx(sound_id);
        g_sound_secondary_weather_sfx_id = 0;
    }

    sound_id = g_sound_tertiary_weather_sfx_id;
    if (sound_id != 0) {
        main_sound_stop_sfx(sound_id);
        g_sound_tertiary_weather_sfx_id = 0;
    }

    /* The target returns an indeterminate saved-register value. */
    return result;
}
