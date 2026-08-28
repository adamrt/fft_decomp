#include "fft/open.h"
#include "psx/types.h"

void open_opcode_wait_counted(s16* entry) {
    u32 flags;
    s32 counter;
    s32 offset;
    open_script_dispatch_state_t* state;

    state = &g_open_script_state.dispatch;
    flags = state->flags;
    entry++;
    if (flags & 4) {
        counter = state->wait_counter + 1;
        state->wait_counter = counter;
        if (counter >= *entry) {
            offset = state->byte_offset;
            state->flags = flags ^ 4;
            state->byte_offset = offset + 4;
        } else {
            state->flags = flags ^ 2;
        }
    } else {
        state->flags = (flags | 4) ^ 2;
        state->wait_counter = 0;
    }
}
