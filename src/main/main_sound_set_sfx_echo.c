#include "fft/main_sound.h"
#include "psx/types.h"

void main_sound_set_sfx_echo(int sound_id, int echo) {
    if (g_main_sound_weather_sfx_id == sound_id && echo == 0) {
        g_main_sound_weather_sfx_id = 0;
    }
    SuzukiSetSfxEcho(sound_id, echo);
}
