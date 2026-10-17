#include "fft/suzuki.h"

/* Copies a saved snapshot over a MUS record and its channels, keeping the
 * record's own queue link and snapshot chain. */
void suzuki_smd_copy_snapshot(suzuki_music_t* music, suzuki_music_t* snapshot) {
    suzuki_music_t* next;
    suzuki_music_t* chain;

    next = music->next;
    chain = music->snapshot;
    suzuki_sound_copy_memory(
        music, snapshot, music->channel_count * sizeof(suzuki_music_channel_t) + sizeof(suzuki_music_t));
    music->next = next;
    music->snapshot = chain;
}
