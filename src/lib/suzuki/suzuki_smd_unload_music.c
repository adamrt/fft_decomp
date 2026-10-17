#include "fft/suzuki.h"

/* Stops a music handle if it is playing, unlinks it from the queue and,
 * when it was queued, frees its snapshots and the record itself. */
void suzuki_smd_unload_music(suzuki_music_t* music) {
    if (music->status & SUZUKI_MUSIC_STATUS_PLAYING) {
        suzuki_smd_stop_music(music);
    }
    if (suzuki_smd_remove_music(music) == 0) {
        suzuki_smd_free_snapshots(music);
        suzuki_sound_free(music);
    }
}
