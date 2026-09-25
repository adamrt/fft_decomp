#include "fft/main.h"
#include "psx/types.h"

/* Advances the play-tune state machine once per frame: a pending tune
 * (bit 7 set) starts once the scenario music has stopped; a playing tune is
 * torn down once it ends and the scenario music is faded back in. The tune
 * at `tune_index` lives in music slot 2 + `tune_index`. */
s32 main_sound_update_tunes(void) {
    s32 tune;
    s32 tune_index;

    tune = g_main_sound_music.slots.tune;
    if (tune & 0x80) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        if (SuzukiGetMusicPlaying(g_main_sound_music.state.current_music) != 0) {
            return 0;
        }
        tune_index = g_main_sound_music.slots.tune & 0x3f;
        g_main_sound_music.slots.tune &= 0x7f;
        tune_handles = (suzuki_music_t**)&g_main_sound_music.slots.handles[2];
        handle = &tune_handles[tune_index];
        if (*handle == 0) {
            *handle = SuzukiPutPlaySMD(g_main_sound_music.slots.smd[2 + tune_index]);
        }
        main_smd_reset_music(*handle, 0x7f, 0);
        return 1;
    }
    if (tune != 0) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        tune_index = tune & 0x3f;
        tune_handles = (suzuki_music_t**)&g_main_sound_music.slots.handles[2];
        handle = &tune_handles[tune_index];
        if (SuzukiGetMusicPlaying(*handle) != 0) {
            return 0;
        }
        SuzukiDeallocateMUSChannels(*handle);
        if (*handle != 0) {
            SuzukiUnloadMUS(*handle);
        }
        *handle = 0;
        g_main_sound_music.slots.tune = 0;
        return main_sound_set_current_music_target(0x7f, 0x78);
    }
    return 0;
}
