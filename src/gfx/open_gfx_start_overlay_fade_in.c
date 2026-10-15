#include "fft/open.h"
#include "psx/types.h"

/* Fade the screen to black over `duration` frames: the subtractive overlay fades
 * in. */
void open_gfx_start_overlay_fade_in(u32 duration) {
    u32 flags = g_open_gfx_overlay_fade.flags;

    g_open_gfx_overlay_fade.duration = duration;
    g_open_gfx_overlay_fade.frame = 0;
    g_open_gfx_overlay_fade.flags = flags | (OPEN_OVERLAY_FADE_IN | OPEN_OVERLAY_FADE_ACTIVE);
}
