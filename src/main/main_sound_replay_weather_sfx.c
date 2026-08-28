#include "fft/main_sound.h"

void main_sound_replay_weather_sfx(void) {
    int sound_id = g_main_sound_weather_sfx_id;

    if (sound_id != 0) {
        SuzukiPlaySound2(sound_id);
    }
}
