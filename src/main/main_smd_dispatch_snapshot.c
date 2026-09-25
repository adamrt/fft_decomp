#include "fft/main.h"
#include "psx/types.h"

/* Discards (0), saves (1) or restores (2) the snapshot of a MUS record. */
void main_smd_dispatch_snapshot(suzuki_music_t* music, s32 mode) {
    switch (mode) {
    case 0:
        main_smd_discard_snapshot(music);
        break;
    case 1:
        main_smd_save_snapshot(music);
        break;
    case 2:
        main_smd_restore_snapshot(music);
        break;
    }
}
