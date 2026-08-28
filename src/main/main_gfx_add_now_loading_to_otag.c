#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

void main_gfx_add_now_loading_to_otag(u32* otag) {
    int i;

    if (g_main_gfx_now_loading_visible != 0) {
        if (((g_main_gfx_now_loading_frame_counter / 60) & 1) == 0) {
            for (i = 0; i < 7; i++) {
                AddPrim(otag, &g_main_gfx_now_loading_primitives[g_main_gfx_screen_polarity][i]);
            }
        }
        g_main_gfx_now_loading_frame_counter++;
    }
}
