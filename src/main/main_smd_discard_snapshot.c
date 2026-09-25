#include "fft/main.h"
#include "psx/types.h"

void main_smd_discard_snapshot(suzuki_music_t* music) {
    u16 status;

    status = music->status;
    if ((status & 0x10) != 0) {
        music->status = status & ~0x10;
        main_smd_free_snapshots(music);
    }
}
