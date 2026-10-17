#include "fft/suzuki.h"
#include "psx/types.h"

/* Sets the bar at which the root-counter handler stops the music. */
void suzuki_smd_set_stop_bar(suzuki_music_t* music, u16 value) {
    music->stop_bar = value;
}
