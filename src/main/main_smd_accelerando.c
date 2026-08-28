#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xA1 "Accelerando": adds a signed tempo delta and
 * clears the scaled tempo so it is recomputed. */
s8* main_smd_accelerando(s8* note_data, suzuki_music_t* music) {
    s32 tempo_delta;
    char unused[8]; /* the target keeps an 8-byte frame */

    (void)unused;
    tempo_delta = note_data[0];
    music->scaled_tempo = 0;
    music->tempo.raw += tempo_delta << 16;
    return note_data + 1;
}
