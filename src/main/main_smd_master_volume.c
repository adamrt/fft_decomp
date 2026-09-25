#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xA6: sets the MUS master volume and
 * marks every channel for a volume update. */
u8* main_smd_master_volume(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->master_volume.value = *note_data++ << 24;
    main_smd_set_note_flags2_all_channels(0x100, music);
    return note_data;
}
