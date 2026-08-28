#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xFD "Scale Tempo". A zero byte leaves the tempo alone. */
u8* main_smd_scale_tempo(u8* note_data, suzuki_music_t* music, void* channel) {
    u32 scale;

    scale = *note_data++;
    if (scale != 0) {
        music->tempo_scale.value = scale << 24;
        music->scaled_tempo = music->tempo.halves.high * (scale << 8);
    }
    return note_data;
}
