#include "fft/main.h"
#include "psx/types.h"

/* Stores the smallest loop count of the music's active channels (0 when
 * none is active) in min_loop_count and returns its address. */
u16* main_smd_get_min_loop_count(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    u16* result;
    u16 count;
    u16 min;

    channel = music->channels;
    count = music->channel_count;
    result = &music->min_loop_count;
    min = 0xffff;
    do {
        if (channel->active != 0) {
            if (channel->loop_count < min) {
                min = channel->loop_count;
            }
        }
        channel++;
    } while (--count != 0);
    if (min == 0xffff) {
        min = 0;
    }
    *result = min;
    return result;
}
