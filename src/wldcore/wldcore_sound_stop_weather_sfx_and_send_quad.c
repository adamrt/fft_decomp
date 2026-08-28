#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/types.h"

enum {
    WLDCORE_SOUND_QUAD_COMMAND_COUNT = 4,
    WLDCORE_SOUND_QUAD_COMMAND_BASE = 0xE10000,
};

/* Stop weather audio and send the selected four-command sound row.
 *
 * Each table byte is added to the command base before the quartet enters the
 * resident Suzuki sound-driver range. The callee's exact operation remains
 * unknown. */
void wldcore_sound_stop_weather_sfx_and_send_quad(s32 sound_id) {
    s32 values[WLDCORE_SOUND_QUAD_COMMAND_COUNT];
    s32 i;

    main_sound_stop_weather_sfx_music();
    for (i = 0; i < WLDCORE_SOUND_QUAD_COMMAND_COUNT; i++) {
        values[i] = g_wldcore_sound_quad_command_bytes[sound_id][i] + WLDCORE_SOUND_QUAD_COMMAND_BASE;
    }
    main_sound_play_4_sfx(values[0], values[1], values[2], values[3]);
}
