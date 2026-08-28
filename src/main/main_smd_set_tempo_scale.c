#include "fft/main_sound.h"
#include "psx/types.h"

/* Sets the music's tempo scale (0x100 = 1.0; 0 means 0x100), immediately
 * when time is 0, otherwise as a ramp over time ticks. */
void main_smd_set_tempo_scale(suzuki_music_t* music, s16 value, s16 time) {
    s32 difference;

    if (value == 0) {
        value = 0x100;
    }
    music->tempo_scale.target = value;
    if (time == 0) {
        music->tempo_scale.value = value << 16;
        music->tempo_scale.count = 0;
        music->scaled_tempo = music->tempo.halves.high * value;
    } else {
        difference = (value << 16) - music->tempo_scale.value;
        if (difference != 0) {
            music->tempo_scale.step = difference / time;
            music->tempo_scale.count = time;
        }
    }
}
