#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xA1 "Accelerando": adds a signed tempo delta and
 * clears the scaled tempo so it is recomputed. */
u8* suzuki_smd_accelerando(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* unused_channel) {
    s32 tempo_delta;
    char unused[8]; /* the target keeps an 8-byte frame */

    (void)unused;
    tempo_delta = *(s8*)note_data;
    music->scaled_tempo = 0;
    music->tempo += tempo_delta << 16;
    return note_data + 1;
}
