#include "fft/main.h"

/* Stops the SFX channels playing sound_id, forgetting it if it is the
 * remembered background sound. */
void main_sound_stop_sfx(int sound_id) {
    if (sound_id == g_main_sound_weather_sfx_id) {
        g_main_sound_weather_sfx_id = 0;
    }
    suzuki_sound_stop_sfx_channels(sound_id);
}
