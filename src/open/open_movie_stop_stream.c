#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

void open_movie_stop_stream(s32 pause_cd_audio) {
    DrawSync(0);
    DecDCToutCallback(0);
    StUnSetRing();

    if (pause_cd_audio != 0) {
        open_movie_pause_cd_audio();
    }

    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING;
}
