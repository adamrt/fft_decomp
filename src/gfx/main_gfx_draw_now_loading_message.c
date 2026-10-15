#include "fft/main.h"

/* Called by main_file_poll_load on every poll of an unfinished read. Unless the
 * counter is -1 (message off), the 64th call unmasks the display and from then on
 * every call draws the blinking message on its own, alternating the two display
 * buffers; the counter stays at 0x40 or 0x41. */
void main_gfx_draw_now_loading_message(void) {
    s32 frame;
    u32* otag;

    frame = g_main_gfx_loading_display_frame_counter;
    if (frame != MAIN_FILE_LOADING_DISPLAY_OFF) {
        frame++;
        g_main_gfx_loading_display_frame_counter = frame;
        if (frame >= 0x40) {
            frame &= 1;
            PutDrawEnv(&g_main_gfx_draw_envs[frame]);
            PutDispEnv(&g_main_gfx_display_envs[frame]);
            otag = g_main_gfx_now_loading_otags[frame];
            ClearOTag(otag, 2);
            main_gfx_add_now_loading_to_otag(otag);
            DrawOTag(otag);
            SetDispMask(1);
            g_main_gfx_loading_display_frame_counter &= 0x41;
        }
    }
}
