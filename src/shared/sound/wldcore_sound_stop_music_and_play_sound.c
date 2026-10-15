#include "fft/wldcore.h"

/* Stop the weather sound and play environment sound `sound_id` as the
 * background sound. */
void wldcore_sound_stop_music_and_play_sound(s32 sound_id) {
    main_sound_stop_weather_sfx_music();
    main_sound_play_bg_sfx(sound_id + SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0));
}
