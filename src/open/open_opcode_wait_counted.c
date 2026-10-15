#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 5: wait command[1] frames. The first call arms the wait and each later
 * call counts one frame, yielding until the count is reached. Four bytes. */
void open_opcode_wait_counted(s16* entry) {
    u32 flags;
    s32 counter;
    open_script_dispatch_state_t* state;

    state = &g_open_script_state.dispatch;
    flags = state->flags;
    entry++;
    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        counter = state->wait_counter + 1;
        state->wait_counter = counter;
        if (counter >= *entry) {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
            state->cursor.byte_offset = state->cursor.byte_offset + 4;
        } else {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        }
    } else {
        state->flags = (flags | OPEN_SCRIPT_DISPATCH_WAITING) ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        state->wait_counter = 0;
    }
}
