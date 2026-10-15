#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Enable the weather sounds and start the rain sound when the weather has set one
 * (g_sound_primary_weather_sfx_id). */
void sound_start_weather_sfx(void) {
    s32 sound_id;

    sound_id = g_sound_primary_weather_sfx_id;
    g_sound_weather_sfx_enabled = 1;
    if (sound_id != 0) {
        main_sound_play_bg_sfx(sound_id);
    }
}
