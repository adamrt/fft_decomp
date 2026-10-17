#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xA0 "Tempo". The root counter ticks 240 times a second and a
 * sequencer tick takes 0x10000 / (tempo * tempo scale) of them, 48 sequencer
 * ticks to a quarter note: at the default scale a tempo byte is worth 75/64
 * quarter notes a minute (the default 0x66 is 119.5). Handlers receive the
 * note-data pointer, the MUS record and the channel; this one only touches
 * the MUS record.
 * `volatile` keeps the redundant `andi 0xff` after the `lbu`. */
u8* suzuki_smd_tempo(u8* data, suzuki_music_t* music, suzuki_music_channel_t* unused_channel) {
    volatile u8* note_data = data;
    u8 tempo;

    tempo = note_data[0] & 0xff;
    music->tempo = tempo << 16;
    music->scaled_tempo = tempo * (music->tempo_scale.value >> 16);
    return data + 1;
}
