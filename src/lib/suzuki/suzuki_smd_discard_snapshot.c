#include "psx/suzuki.h"
#include "psx/types.h"

/* Drop a music record's saved snapshot without restoring it: clear SUZUKI_MUSIC_STATUS_SNAPSHOT
 * and free the snapshot chain. */
void suzuki_smd_discard_snapshot(suzuki_music_t* music) {
    u16 status;

    status = music->status;
    if ((status & SUZUKI_MUSIC_STATUS_SNAPSHOT) != 0) {
        music->status = status & ~SUZUKI_MUSIC_STATUS_SNAPSHOT;
        suzuki_smd_free_snapshots(music);
    }
}
