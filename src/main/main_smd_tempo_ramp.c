#include "fft/main_sound.h"
#include "psx/types.h"

u8* main_smd_tempo_ramp(u8* note_data, suzuki_music_t* music, void* channel) {
    s16 steps;
    u32 target;
    u32 scaled;
    s32 delta;

    target = note_data[1];
    steps = note_data[0];
    music->tempo_target = target;
    scaled = target << 16;
    delta = scaled - music->tempo.raw;
    if (steps != 0 && delta != 0) {
        music->tempo_step_count = steps;
        music->tempo_step = delta / steps;
    }
    return note_data + 2;
}
