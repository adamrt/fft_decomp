#include "fft/main_gfx.h"
#include "psx/gpu.h"

int main_gfx_swap_display_area(u32 otag) {
    /* Matching constraint: the retail function reserves one unused RECT. */
    volatile RECT matching_stack_space;
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
