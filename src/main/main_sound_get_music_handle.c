#include "fft/main.h"

int main_sound_get_music_handle(int slot) {
    return g_main_sound_music.slots.handles[slot];
}
