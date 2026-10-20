/* Resident access to the active BATTLE ordering tables. */

#include "fft/gfx.h"
#include "fft/render.h"
#include "psx/libgpu.h"

/* Return the ordering table of BATTLE's current render buffer; the pointer is a BATTLE.BIN global,
 * so this only works while BATTLE is loaded. */
u32* gfx_battle_otag_get(void) {
    return g_render_current_buffer->otag;
}

/* Make the other of BATTLE's two render buffers the current one and clear its
 * 0x180-entry ordering table. */
void gfx_battle_otag_swap(void) {
    render_buffer_t* buffer = g_render_buffers;
    u32* otag;

    if (g_render_current_buffer == buffer) {
        buffer++;
    }
    otag = buffer->otag;
    g_render_current_buffer = buffer;
    ClearOTagR(otag, 0x180);
}
