#include "fft/open.h"
#include "psx/types.h"

void open_opcode_start_overlay_fade_out(s16* command) {
    s32* script_offset;

    command++;
    g_open_gfx_overlay_fade.ot_index = 2;
    open_gfx_start_overlay_fade_out(*command);
    script_offset = &g_open_script_state.dispatch.byte_offset;
    *script_offset += 4;
}
