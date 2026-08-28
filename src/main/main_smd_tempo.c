#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xA0 "Tempo". Handlers receive the note-data pointer,
 * the MUS record and the channel; this one only touches the MUS record.
 * `volatile` keeps the redundant `andi 0xff` after the `lbu`. */
u8* main_smd_tempo(volatile u8* note_data, suzuki_music_t* music) {
    u16 tempo;

    tempo = (u16)(note_data[0] & 0xff);
    music->tempo.raw = tempo << 16;
    music->scaled_tempo = (u8)tempo * (s16)(music->tempo_scale.value >> 16);
    return note_data + 1;
}
