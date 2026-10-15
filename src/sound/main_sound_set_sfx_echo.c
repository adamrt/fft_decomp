#include "fft/main.h"

/* Sets the volume of the SFX channels playing sound_id: volume is the new
 * channel velocity, and 0 stops them (and forgets a remembered background
 * sound). */
void main_sound_set_sfx_echo(int sound_id, int volume) {
    if (g_main_sound_weather_sfx_id == sound_id && volume == 0) {
        g_main_sound_weather_sfx_id = 0;
    }
    suzuki_sound_set_sfx_volume(sound_id, volume);
}
