#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 6: wait until the text has scrolled command[1] pixels, counted from the
 * call that arms the wait. Four bytes. */
void open_opcode_wait_for_external_counter(s16* entry) {
    u32 flags;
    open_script_dispatch_state_t* state;

    state = &g_open_script_state.dispatch;
    flags = state->flags;
    entry++;
    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (state->external_counter >= *entry) {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
            state->cursor.byte_offset = state->cursor.byte_offset + 4;
        } else {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        }
    } else {
        state->flags = (flags | OPEN_SCRIPT_DISPATCH_WAITING) ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        state->external_counter = 0;
    }
}
