#include "fft/event_require.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Fade g_require_gfx_fade_rgb linearly from its current value to eight times
 * each target over 31 frames, yielding once per frame. */
void require_gfx_fade_rgb_31_frames(s32 target_0, s32 target_1, s32 target_2) {
    s32 current[3];
    s32 interpolated[3];
    s32 target[3];
    s32 frame;
    s32 i;

    frame = 0;
    target[0] = target_0 * 8;
    target[1] = target_1 * 8;
    target[2] = target_2 * 8;
    current[0] = g_require_gfx_fade_rgb[0];
    current[1] = g_require_gfx_fade_rgb[1];
    current[2] = g_require_gfx_fade_rgb[2];
    do {
        for (i = 0; i < 3; i++) {
            interpolated[i] = (((target[i] - current[i]) * frame) / 30) + current[i];
            g_require_gfx_fade_rgb[i] = interpolated[i];
        }
        frame++;
        thread_yield();
    } while (frame < 31);
}
