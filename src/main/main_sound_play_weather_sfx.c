#include "fft/main.h"

void main_sound_play_weather_sfx(int sound_id) {
    g_main_sound_weather_sfx_id = sound_id;
    SuzukiPlaySound2(sound_id);
}
