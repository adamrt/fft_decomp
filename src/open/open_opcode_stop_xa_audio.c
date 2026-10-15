#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 10: stop the XA audio and wait until the stop sequence of
 * open_script_update_xa_audio has finished. The opcode is skipped when XA audio
 * is neither playing nor starting. Two bytes. */
void open_opcode_stop_xa_audio(s16* unused_entry) {
    u32 state = g_open_script_state.dispatch.flags;

    if (state & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (state & OPEN_SCRIPT_DISPATCH_XA_STOPPING) {
            g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        } else {
            g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_WAITING;
            g_open_script_state.dispatch.cursor.byte_offset += 2;
        }
    } else if (!(state & (OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STARTING))) {
        g_open_script_state.dispatch.cursor.byte_offset += 2;
    } else {
        g_open_script_state.control_state = 0;
        g_open_script_state.dispatch.flags
            = (state
                  & ~(OPEN_SCRIPT_DISPATCH_CONTINUE | OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STOPPING
                      | OPEN_SCRIPT_DISPATCH_XA_STARTING))
            | (OPEN_SCRIPT_DISPATCH_WAITING | OPEN_SCRIPT_DISPATCH_XA_STOPPING);
        open_sound_set_cd_mix_and_volume(0, 1);
    }
}
