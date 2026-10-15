#include "fft/battle_gfx.h"
#include "fft/main.h"
#include "psx/types.h"

/* Make the other of BATTLE's two render buffers the current one and clear its
 * 0x180-entry ordering table. */
void main_gfx_swap_and_clear_otag(void) {
    battle_render_buffer_t* stats = g_battle_render_buffers;
    u32* otag;

    if (g_battle_gfx_current_render_buffer == stats) {
        stats++;
    }
    otag = stats->otag;
    g_battle_gfx_current_render_buffer = stats;
    ClearOTagR(otag, 0x180);
}
