#include "fft/battle_gfx.h"
#include "fft/main.h"

u32* main_gfx_get_otag(void) {
    return g_battle_gfx_current_render_buffer->otag;
}
