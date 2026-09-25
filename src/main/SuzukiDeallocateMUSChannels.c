#include "fft/main.h"
#include "psx/types.h"

void SuzukiDeallocateMUSChannels(suzuki_music_t* music) {
    if (music != 0) {
        music->status &= 0x7fff;
        g_main_sound_music_key_off_voices |= SuzukiGetActiveChannels(music);
    }
}
