/* The blinking Now Loading message drawn while a read is unfinished. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/system.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static u32 g_loading_otags[2][2] = { 0 };

/* Called by file_poll_load on every poll of an unfinished read. Unless the
 * counter is -1 (message off), the 64th call unmasks the display and from then on
 * every call draws the blinking message on its own, alternating the two display
 * buffers; the counter stays at 0x40 or 0x41. */
void loading_draw_message(void) {
    s32 frame = g_loading_display_frame_counter;
    u32* otag;

    if (frame != FILE_LOADING_DISPLAY_OFF) {
        frame++;
        g_loading_display_frame_counter = frame;
        if (frame >= 0x40) {
            frame &= 1;
            PutDrawEnv(&g_gfx_draw_envs[frame]);
            PutDispEnv(&g_gfx_display_envs[frame]);
            otag = g_loading_otags[frame];
            ClearOTag(otag, 2);
            loading_add_to_otag(otag);
            DrawOTag(otag);
            SetDispMask(1);
            g_loading_display_frame_counter &= 0x41;
        }
    }
}
