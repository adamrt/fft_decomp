#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xFE "Select Sound Font": stores the waveset id in the
 * MUS record and gives the channel the loaded waveset with that id, or the
 * first loaded waveset when none matches. id is s16 like the parameter of
 * main_sound_find_waveset, whose search loop this repeats. */
u8* main_smd_select_sound_font(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_waveset_t* waveset;
    s16 id;

    waveset = g_main_sound_waveset_list;
    id = *note_data++;
    music->waveset_id = id;
    while (waveset != 0 && waveset->id != id) {
        waveset = waveset->next;
    }
    if (waveset == 0) {
        waveset = g_main_sound_waveset_list;
    }
    channel->waveset = waveset;
    return note_data;
}
