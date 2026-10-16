#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xFD "Scale Tempo": multiplies the tempo by its byte. A zero
 * byte leaves the tempo alone. */
u8* suzuki_smd_scale_tempo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 scale = *note_data++;

    if (scale != 0) {
        music->tempo_scale.value = scale << 24;
        music->scaled_tempo = (music->tempo >> 16) * (scale << 8);
    }
    return note_data;
}
