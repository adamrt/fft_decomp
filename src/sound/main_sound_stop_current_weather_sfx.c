#include "fft/main.h"

/* Stops the remembered background sound but keeps its id, so
 * main_sound_replay_bg_sfx can restart it. */
void main_sound_stop_current_weather_sfx(void) {
    int sound_id = g_main_sound_weather_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_stop_sfx_channels(sound_id);
    }
}
