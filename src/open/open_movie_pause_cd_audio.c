#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* While the CD audio plays: drop its volume to 0, pause the drive, wait four
 * vertical blanks and clear OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING. */
void open_movie_pause_cd_audio(void) {
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) != 0) {
        open_sound_set_cd_mix_and_volume(0, 1);

        while (CdControlB(CdlPause, 0, 0) == 0) { }

        VSync(4);
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING;
    }
}
