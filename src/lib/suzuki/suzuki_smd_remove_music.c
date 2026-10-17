#include "fft/suzuki.h"
#include "psx/types.h"

/* Unlinks a MUS record from the active music list. A playing record is
 * stopped first and its voices are queued for key-off. Returns -1 when the
 * record is not queued, otherwise 0.
 *
 * The target reads status with `lh` and tests the record pointer only after
 * that read. */
s32 suzuki_smd_remove_music(suzuki_music_t* music) {
    suzuki_music_t* entry;
    suzuki_music_t* previous = 0;
    s16 status;

    entry = g_suzuki_sound_active_music_list;
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
    if ((status & SUZUKI_MUSIC_STATUS_PLAYING) && music != 0) {
        music->status = status & ~SUZUKI_MUSIC_STATUS_PLAYING;
        g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
    }
    if (previous != 0) {
        previous->next = music->next;
    } else {
        g_suzuki_sound_active_music_list = music->next;
    }
    return 0;
}
