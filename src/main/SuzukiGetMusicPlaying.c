#include "fft/main_sound.h"
#include "psx/types.h"

u32 SuzukiGetMusicPlaying(suzuki_music_t* music) {
    return music->status >> 15;
}
