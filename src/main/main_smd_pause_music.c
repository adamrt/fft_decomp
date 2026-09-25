#include "fft/main.h"
#include "psx/types.h"

/* Pauses a music handle: clears the playing bit, sets the pause bit 0x100
 * that main_smd_resume_music clears again, and queues the voices
 * of its active channels for key-off. */
void main_smd_pause_music(suzuki_music_t* music) {
    if (music != 0) {
        music->status = (music->status & 0x7fff) | 0x100;
        g_main_sound_music_key_off_voices |= SuzukiGetActiveChannels(music);
    }
}
