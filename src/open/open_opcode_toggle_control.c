#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 0: end the script. It yields for the frame while the XA start or stop
 * sequence runs; after that it clears OPEN_SCRIPT_DISPATCH_RUNNING, which stops
 * the dispatcher. Two bytes. */
void open_opcode_toggle_control(s16* unused_entry) {
    u32 state = g_open_script_state.dispatch.flags;

    if (state & (OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING)) {
        g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
    } else {
        g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_RUNNING;
        g_open_script_state.dispatch.cursor.byte_offset += 2;
    }
}
