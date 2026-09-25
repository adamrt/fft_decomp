#include "fft/main.h"
#include "psx/types.h"

/* Fills time with the elapsed play time of a music record. */
void main_smd_get_play_time(suzuki_music_t* music, suzuki_play_time_t* time) {
    u32 ticks;
    u32 seconds;

    ticks = music->ticks >> 8;
    seconds = ticks / 240;
    time->tick_24 = music->tick_24;
    time->fraction = ticks % 240;
    time->seconds = seconds % 60;
    time->minutes = seconds / 60;
}
