#include "fft/main.h"

void main_sound_stop_current_weather_sfx(void) {
    int sound_id = g_main_sound_weather_sfx_id;

    if (sound_id != 0) {
        main_sound_stop_sfx_channels(sound_id);
    }
}
