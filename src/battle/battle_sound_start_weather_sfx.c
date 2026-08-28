#include "fft/battle.h"
#include "psx/types.h"

void battle_sound_start_weather_sfx(void) {
    s32 sound_id;

    sound_id = g_battle_sound_primary_weather_sfx_id;
    g_battle_sound_weather_sfx_enabled = 1;
    if (sound_id != 0) {
        main_sound_play_weather_sfx(sound_id);
    }
}
