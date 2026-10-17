#include "fft/suzuki.h"
#include "psx/types.h"

u32 suzuki_smd_is_music_playing(suzuki_music_t* music) {
    return (music->status & SUZUKI_MUSIC_STATUS_PLAYING) != 0;
}
