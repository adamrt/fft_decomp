#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xA2 "Tempo Ramp": moves the tempo to the target byte over the
 * given number of sequencer ticks (suzuki_smd_update_channel_ramps steps it).
 * A zero count or an unchanged tempo starts nothing. */
u8* suzuki_smd_tempo_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s16 steps;
    u32 target = note_data[1];
    u32 scaled;
    s32 delta;

    steps = note_data[0];
    music->tempo_target = target;
    scaled = target << 16;
    delta = scaled - music->tempo;
    if (steps != 0 && delta != 0) {
        music->tempo_step_count = steps;
        music->tempo_step = delta / steps;
    }
    return note_data + 2;
}
