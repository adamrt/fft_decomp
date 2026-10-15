#include "fft/main.h"

/* Plays a background sound on SFX channels 4-5 and remembers its id in
 * g_main_sound_weather_sfx_id, so main_sound_stop_current_weather_sfx can
 * stop it and main_sound_replay_bg_sfx can start it again. */
void main_sound_play_bg_sfx(int sound_id) {
    g_main_sound_weather_sfx_id = sound_id;
    suzuki_sound_play_background_sfx(sound_id);
}
