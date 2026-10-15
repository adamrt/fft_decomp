#include "fft/main.h"

/* Plays a sound on SFX channels 6-7 at the highest priority, replacing
 * whatever those channels play. */
void main_sound_play_sfx(int sound_id) {
    suzuki_sound_play_direct_sfx(sound_id);
}

/* Plays a sound on a free SFX channel pair, or on the oldest stealable one. */
void main_sound_play_sfx_find_channel(int sound_id) {
    suzuki_sound_play_sfx(sound_id);
}

/* Plays a background sound on SFX channels 4-5 and remembers its id in
 * g_main_sound_weather_sfx_id, so main_sound_stop_current_weather_sfx can
 * stop it and main_sound_replay_bg_sfx can start it again. */
void main_sound_play_bg_sfx(int sound_id) {
    g_main_sound_weather_sfx_id = sound_id;
    suzuki_sound_play_background_sfx(sound_id);
}

/* Starts the remembered background sound again. */
void main_sound_replay_bg_sfx(void) {
    int sound_id = g_main_sound_weather_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_play_background_sfx(sound_id);
    }
}

/* Sets the volume of the SFX channels playing sound_id: volume is the new
 * channel velocity, and 0 stops them (and forgets a remembered background
 * sound). */
void main_sound_set_sfx_echo(int sound_id, int volume) {
    if (g_main_sound_weather_sfx_id == sound_id && volume == 0) {
        g_main_sound_weather_sfx_id = 0;
    }
    suzuki_sound_set_sfx_volume(sound_id, volume);
}

/* Forgets the remembered background sound and silences every SFX channel.
 * It leaves the music alone. */
void main_sound_stop_weather_sfx_music(void) {
    g_main_sound_weather_sfx_id = 0;
    suzuki_sound_stop_all_sfx();
}

/* Stops the SFX channels playing sound_id, forgetting it if it is the
 * remembered background sound. */
void main_sound_stop_sfx(int sound_id) {
    if (sound_id == g_main_sound_weather_sfx_id) {
        g_main_sound_weather_sfx_id = 0;
    }
    suzuki_sound_stop_sfx_channels(sound_id);
}

/* Stops the remembered background sound but keeps its id, so
 * main_sound_replay_bg_sfx can restart it. */
void main_sound_stop_current_weather_sfx(void) {
    int sound_id = g_main_sound_weather_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_stop_sfx_channels(sound_id);
    }
}
