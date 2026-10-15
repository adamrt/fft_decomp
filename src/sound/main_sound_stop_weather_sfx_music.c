#include "fft/main.h"

/* Forgets the remembered background sound and silences every SFX channel.
 * It leaves the music alone. */
void main_sound_stop_weather_sfx_music(void) {
    g_main_sound_weather_sfx_id = 0;
    suzuki_sound_stop_all_sfx();
}
