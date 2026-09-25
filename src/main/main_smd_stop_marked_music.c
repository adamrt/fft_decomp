#include "fft/main.h"
#include "psx/types.h"

/* Stops every queued music record whose status bit 0 is set, queueing the
 * voices of its active channels for key-off. */
void main_smd_stop_marked_music(void) {
    suzuki_music_t* music;

    music = g_main_sound_active_music_list;
    while (music != 0) {
        if (music->status & 1) {
            music->status &= 0x7fff;
            g_main_sound_music_key_off_voices |= SuzukiGetActiveChannels(music);
        }
        music = music->next;
    }
}
