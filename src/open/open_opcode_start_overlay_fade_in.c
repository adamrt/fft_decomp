#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 4: fade the screen to black over command[1] frames, with the overlay in
 * ordering-table entry 2. Four bytes. */
void open_opcode_start_overlay_fade_in(s16* command) {
    command++;
    g_open_gfx_overlay_fade.ot_index = 2;
    open_gfx_start_overlay_fade_in(*command);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}
