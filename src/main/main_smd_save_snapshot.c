#include "fft/main.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Copies the MUS record and its channels into the record's snapshot block,
 * allocating the block on first use, with the root-counter event disabled. */
void main_smd_save_snapshot(suzuki_music_t* music) {
    suzuki_music_t* snapshot;
    u32 size;

    DisableEvent(g_main_root_counter_2_event);
    music->status |= 0x10;
    size = music->channel_count * sizeof(suzuki_music_channel_t)
        + (sizeof(suzuki_music_t) - sizeof(suzuki_music_channel_t));
    if (music->snapshot == 0) {
        music->snapshot = main_sound_alloc(size);
    }
    snapshot = music->snapshot;
    main_sound_copy_memory(snapshot, music, size);
    snapshot->next = 0;
    snapshot->snapshot = 0;
    music->tick_2c = 0;
    EnableEvent(g_main_root_counter_2_event);
}
