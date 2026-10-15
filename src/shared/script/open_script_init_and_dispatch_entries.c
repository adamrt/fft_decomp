#include "fft/open.h"

/* Run one frame of the ending script. Returns 0 once the script has ended and 1
 * while it runs.
 *
 * Updates the text scroll, the XA audio, the OPNTEX sequence and the picture
 * fade, then executes script entries. An entry is a row of 16-bit values at
 * data_base + byte_offset whose first value, the opcode, indexes
 * g_open_opcode_handlers. A handler advances byte_offset itself; one that has to
 * wait clears OPEN_SCRIPT_DISPATCH_CONTINUE to end the frame and is entered again
 * on the next, and opcode 0 clears OPEN_SCRIPT_DISPATCH_RUNNING. */
s32 open_script_init_and_dispatch_entries(void) {
    open_script_dispatch_state_t* state;
    open_script_cursor_t* cursor;
    s32 result;
    s16* entry;
    u32 flags;

    open_script_update_timing_and_record_values();
    open_script_update_xa_audio();
    open_gfx_update_opntex_sequence();
    open_script_update_screen_fade();

    state = &g_open_script_state.dispatch;
    result = state->flags | OPEN_SCRIPT_DISPATCH_CONTINUE;
    state->flags = result;
    if (result & OPEN_SCRIPT_DISPATCH_CONTINUE) {
        cursor = &state->cursor;
        do {
            entry = (s16*)open_script_add_pointer_offset(cursor->data_base, cursor->byte_offset);
            g_open_opcode_handlers[*entry](entry);
            /* Keeping the cursor base live matches retail; the direct state view is four bytes shorter. */
            flags = ((open_script_dispatch_state_t*)((u8*)cursor - offsetof(open_script_dispatch_state_t, cursor)))
                        ->flags;
            if ((flags & OPEN_SCRIPT_DISPATCH_RUNNING) == 0) {
                return 0;
            }
        } while (flags & OPEN_SCRIPT_DISPATCH_CONTINUE);
    }
    return 1;
}
