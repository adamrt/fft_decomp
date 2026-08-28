#include "fft/open.h"
#include "psx/types.h"

void open_movie_pause_cd_audio(void) {
    if ((g_open_system_runtime_flags & 4) != 0) {
        open_sound_set_type_and_volume(0, 1);

        while (CdControlb(9, 0, 0) == 0) { }

        VSync(4);
        g_open_system_runtime_flags ^= 4;
    }
}
