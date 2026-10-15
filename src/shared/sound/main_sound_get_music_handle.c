#include "fft/main.h"

suzuki_music_t* main_sound_get_music_handle(int slot) {
    return g_main_sound_music.slots.music[slot - 1];
}
