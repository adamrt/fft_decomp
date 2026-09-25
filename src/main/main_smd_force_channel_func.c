#include "fft/main.h"
#include "psx/types.h"

/* ORs mask into func_flags of every channel that is in use
 * (callers pass 0x7000 / 0x71ff). */
void main_smd_force_channel_func(suzuki_music_t* music, u16 mask) {
    suzuki_music_channel_t* channel;
    u16 func_flags;
    s32 count;

    channel = music->channels;
    count = music->channel_count;
    do {
        if (channel->active != 0) {
            func_flags = channel->func_flags; /* temp keeps `or rd,flags,mask` operand order */
            channel->func_flags = func_flags | mask;
        }
        channel++;
    } while (--count != 0);
}
