#include "fft/main.h"
#include "psx/types.h"

/* Detaches the snapshot chain of a MUS record and frees every block in it.
 *
 * It follows the snapshot links (+0x04), not the queue; the queue removal is
 * main_smd_remove_music. The target keeps music and the next link in one
 * register, so music is reused for the link. */
void main_smd_free_snapshots(suzuki_music_t* music) {
    suzuki_music_t* snapshot;

    snapshot = music->snapshot;
    if (snapshot != 0) {
        music->snapshot = 0;
        do {
            music = snapshot->snapshot;
            main_sound_free(snapshot);
            snapshot = music;
        } while (music != 0);
    }
}
