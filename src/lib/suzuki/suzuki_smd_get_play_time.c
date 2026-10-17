#include "fft/suzuki.h"
#include "psx/types.h"

/* Fills time with the elapsed play time of a music record. */
void suzuki_smd_get_play_time(suzuki_music_t* music, suzuki_play_time_t* time) {
    u32 ticks;
    u32 seconds;

    ticks = music->ticks >> 8;
    seconds = ticks / 240;
    time->sequence_ticks = music->sequence_ticks;
    time->fraction = ticks % 240;
    time->seconds = seconds % 60;
    time->minutes = seconds / 60;
}
