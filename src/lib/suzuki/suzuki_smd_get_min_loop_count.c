#include "fft/suzuki.h"
#include "psx/types.h"

/* Stores the smallest loop count of the music's active channels (0 when
 * none is active) in min_loop_count and returns its address. */
u16* suzuki_smd_get_min_loop_count(suzuki_music_t* music) {
    suzuki_music_channel_t* channel = music->channels;
    u16* result;
    u16 count = music->channel_count;
    u16 min;

    result = &music->min_loop_count;
    min = 0xffff;
    do {
        if (channel->flags != 0) {
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
