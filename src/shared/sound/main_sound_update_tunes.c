#include "fft/main.h"
#include "psx/types.h"

/* Advances the play-tune state machine once per frame: a pending tune
 * (MAIN_SOUND_TUNE_PENDING) starts once the scenario music has stopped; a playing tune is
 * torn down once it ends and the scenario music is faded back in. The tune
 * at `tune_index` lives in music slot 2 + `tune_index`. */
s32 main_sound_update_tunes(void) {
    s32 tune;
    s32 tune_index;

    tune = g_main_sound_music.slots.tune;
    if (tune & MAIN_SOUND_TUNE_PENDING) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        if (suzuki_smd_is_music_playing(g_main_sound_music.state.current_music) != 0) {
            return 0;
        }
        tune_index = g_main_sound_music.slots.tune & MAIN_SOUND_TUNE_ID_MASK;
        g_main_sound_music.slots.tune &= 0x7f;
        tune_handles = &g_main_sound_music.slots.music[1];
        handle = &tune_handles[tune_index];
        if (*handle == 0) {
            *handle = suzuki_smd_create_music(g_main_sound_music.slots.smd[1 + tune_index]);
        }
        suzuki_smd_reset_music(*handle, 0x7f, 0);
        return 1;
    }
    if (tune != 0) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        tune_index = tune & MAIN_SOUND_TUNE_ID_MASK;
        tune_handles = &g_main_sound_music.slots.music[1];
        handle = &tune_handles[tune_index];
        if (suzuki_smd_is_music_playing(*handle) != 0) {
            return 0;
        }
        suzuki_smd_stop_music(*handle);
        if (*handle != 0) {
            suzuki_smd_unload_music(*handle);
        }
        *handle = 0;
        g_main_sound_music.slots.tune = 0;
        return main_sound_set_current_music_target(0x7f, 0x78);
    }
    return 0;
}
