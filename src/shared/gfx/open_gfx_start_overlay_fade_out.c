#include "fft/open.h"
#include "psx/types.h"

/* Reveal the screen over `duration` frames: the subtractive overlay fades out. */
void open_gfx_start_overlay_fade_out(u32 duration) {
    u32 flags = g_open_gfx_overlay_fade.flags;

    g_open_gfx_overlay_fade.duration = duration;
    g_open_gfx_overlay_fade.frame = 0;
    g_open_gfx_overlay_fade.flags = flags | (OPEN_OVERLAY_FADE_OUT | OPEN_OVERLAY_FADE_ACTIVE);
}
