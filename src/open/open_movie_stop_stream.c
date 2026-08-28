#include "fft/open.h"
#include "psx/types.h"

void open_movie_stop_stream(s32 pause_cd_audio) {
    DrawSync(0);
    open_bin_decdctoutcallback(0);
    StSetDataReadyCallback();

    if (pause_cd_audio != 0) {
        open_movie_pause_cd_audio();
    }

    g_open_system_runtime_flags &= ~2;
}
