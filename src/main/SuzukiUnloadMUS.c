#include "fft/main.h"
#include "psx/types.h"

/* Stops a music handle if it is playing, unlinks it from the queue and,
 * when it was queued, frees its snapshots and the record itself. */
void SuzukiUnloadMUS(suzuki_music_t* music) {
    /* The target tests the playing bit on an `lh` load. */
    if ((s16)music->status & 0x8000) {
        SuzukiDeallocateMUSChannels(music);
    }
    if (main_smd_remove_music(music) == 0) {
        main_smd_free_snapshots(music);
        main_sound_free(music);
    }
}
