#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xB8 "Acoustics": reverb depth, delay and feedback,
 * applied at once with reverb mode 10. */
u8* main_smd_acoustics(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s16 depth;
    s32 delay;
    s32 feedback;

    depth = note_data[0] << 8;
    music->reverb_depth = depth;
    delay = (s8)note_data[1];
    music->reverb_delay = delay;
    feedback = (s8)note_data[2];
    music->reverb_feedback = feedback;
    main_sound_set_reverb_mode(10, depth, delay, feedback);
    return note_data + 3;
}
