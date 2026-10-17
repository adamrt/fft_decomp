#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Copies the MUS record and its channels into the record's snapshot block,
 * allocating the block on first use, with the root-counter event disabled. */
void suzuki_smd_save_snapshot(suzuki_music_t* music) {
    suzuki_music_t* snapshot;
    u32 size;

    DisableEvent(g_suzuki_sound_root_counter_2_event);
    music->status |= SUZUKI_MUSIC_STATUS_SNAPSHOT;
    size = music->channel_count * sizeof(suzuki_music_channel_t) + sizeof(suzuki_music_t);
    if (music->snapshot == 0) {
        music->snapshot = suzuki_sound_alloc(size);
    }
    snapshot = music->snapshot;
    suzuki_sound_copy_memory(snapshot, music, size);
    snapshot->next = 0;
    snapshot->snapshot = 0;
    music->snapshot_restore_tick = 0;
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}
