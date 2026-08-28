#include "fft/main_sound.h"
#include "psx/types.h"

/* Unlinks a MUS record from the active music list. A playing record is
 * stopped first and its voices are queued for key-off. Returns -1 when the
 * record is not queued, otherwise 0.
 *
 * The target reads status with `lh` and tests the record pointer only after
 * that read. */
s32 main_smd_remove_music(suzuki_music_t* music) {
    suzuki_music_t* entry;
    suzuki_music_t* previous;
    s16 status;

    previous = 0;
    entry = g_main_sound_active_music_list;
    while (entry != 0) {
        if (entry == music) {
            break;
        }
        previous = entry;
        entry = entry->next;
    }
    if (entry == 0) {
        return -1;
    }
    status = music->status;
    if ((status & 0x8000) && music != 0) {
        music->status = status & 0x7fff;
        g_main_sound_music_key_off_voices |= SuzukiGetActiveChannels(music);
    }
    if (previous != 0) {
        previous->next = music->next;
    } else {
        g_main_sound_active_music_list = music->next;
    }
    return 0;
}
