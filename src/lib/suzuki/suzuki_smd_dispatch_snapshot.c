#include "fft/suzuki.h"
#include "psx/types.h"

/* Discards (0), saves (1) or restores (2) the snapshot of a MUS record. */
void suzuki_smd_dispatch_snapshot(suzuki_music_t* music, s32 mode) {
    switch (mode) {
    case SUZUKI_SNAPSHOT_DISCARD:
        suzuki_smd_discard_snapshot(music);
        break;
    case SUZUKI_SNAPSHOT_SAVE:
        suzuki_smd_save_snapshot(music);
        break;
    case SUZUKI_SNAPSHOT_RESTORE:
        suzuki_smd_restore_snapshot(music);
        break;
    }
}
