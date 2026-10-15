#include "fft/open.h"
#include "psx/types.h"

/* Clear the active buffer's 16-entry ordering table and release the frame's
 * sprite primitives. */
void open_gfx_clear_current_otag(void) {
    u32 stack_padding[2];

    ClearOTagR(g_open_gfx_otags[g_active_graphics_buffer_index], 0x10);
    g_open_gfx_primitive_count = 0;
}
