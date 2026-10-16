#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x9C: play a sound effect. Little-endian sound id, then a
 * volume byte; the balance is centered. */
u8* suzuki_smd_play_sfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_sound_play_sfx_with_settings_find_channel(note_data[0] | (note_data[1] << 8), note_data[2], 0x40);
    return note_data + 3;
}
