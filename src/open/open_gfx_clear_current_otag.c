#include "fft/main_runtime.h"
#include "fft/open.h"
#include "psx/types.h"

void open_gfx_clear_current_otag(void) {
    volatile u32 stack_padding[2];

    ClearOTagR(g_open_gfx_otags[g_active_graphics_buffer_index], 0x10);
    g_open_gfx_primitive_count = 0;
}
