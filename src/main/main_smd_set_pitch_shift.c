#include "fft/main.h"
#include "psx/types.h"

/* Sets the music's pitch shift, immediately when time is 0, otherwise as a ramp
 * over time ticks. */
void main_smd_set_pitch_shift(suzuki_music_t* music, s16 value, s16 time) {
    s32 difference;

    music->pitch_shift.target = value << 8;
    if (time == 0) {
        music->pitch_shift.value = value << 24;
        music->pitch_shift.count = 0;
        main_smd_set_note_flags2_all_channels(0x200, music);
    } else {
        difference = (value << 16) - (music->pitch_shift.value >> 8);
        if (difference != 0) {
            music->pitch_shift.count = time;
            music->pitch_shift.step = (difference / time) << 8;
        }
    }
}
