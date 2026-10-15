#include "fft/wldcore.h"
#include "psx/types.h"

enum {
    WLDCORE_SOUND_QUAD_COMMAND_COUNT = 4,
    WLDCORE_SOUND_QUAD_COMMAND_BASE = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_SOUND_NOVEL, 0),
};

/* Stops the weather sound and plays row sound_id of
 * g_wldcore_sound_quad_command_bytes: four sounds of the sound novel's
 * resource, one on each SFX channel pair. A zero byte selects that
 * resource's empty sound 0, which only stops its channel pair. */
void wldcore_sound_stop_weather_sfx_and_send_quad(s32 sound_id) {
    s32 values[WLDCORE_SOUND_QUAD_COMMAND_COUNT];
    s32 i;

    main_sound_stop_weather_sfx_music();
    for (i = 0; i < WLDCORE_SOUND_QUAD_COMMAND_COUNT; i++) {
        values[i] = g_wldcore_sound_quad_command_bytes[sound_id][i] + WLDCORE_SOUND_QUAD_COMMAND_BASE;
    }
    suzuki_sound_play_4_sfx(values[0], values[1], values[2], values[3]);
}
