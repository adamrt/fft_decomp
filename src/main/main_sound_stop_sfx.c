#include "fft/main.h"

void main_sound_stop_sfx(int sound_id) {
    if (sound_id == g_main_sound_weather_sfx_id) {
        g_main_sound_weather_sfx_id = 0;
    }
    main_sound_stop_sfx_channels(sound_id);
}
