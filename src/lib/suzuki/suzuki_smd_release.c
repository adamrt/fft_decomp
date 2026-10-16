#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xC5 "Release Rate". The byte lands both in
 * note_release_rate, which every note reloads, and in release_rate. */
u8* suzuki_smd_release(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 release = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_RELEASE;
    channel->note_release_rate = release;
    channel->voice_attr.rr = release;
    return note_data + 1;
}
