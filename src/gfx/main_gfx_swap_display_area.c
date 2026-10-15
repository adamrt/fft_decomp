#include "fft/main.h"
#include "psx/libgpu.h"

/* Present a frame: flip g_main_gfx_screen_polarity, wait for the GPU and VSync,
 * apply the new buffer's display and draw environments and draw otag. Returns
 * VSync's result. */
int main_gfx_swap_display_area(u32* otag) {
    /* Matching constraint: the retail function reserves one unused RECT. */
    RECT matching_stack_space;
    int elapsed;

    g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}
