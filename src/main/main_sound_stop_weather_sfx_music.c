#include "fft/main_sound.h"

void main_sound_stop_weather_sfx_music(void) {
    g_main_sound_weather_sfx_id = 0;
    SuzukiTurnOffAllMusic();
}
