#include "fft/main.h"
#include "psx/types.h"

/* Returns the loaded waveset with the given id, or 0. */
suzuki_waveset_t* main_sound_find_waveset(s16 id) {
    suzuki_waveset_t* waveset;

    waveset = g_main_sound_waveset_list;
    while ((waveset != 0) && (waveset->id != id)) {
        waveset = waveset->next;
    }
    return waveset;
}
