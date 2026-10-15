#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 7: start channel command[1] of ENDING.XA and wait until the start
 * sequence of open_script_update_xa_audio has finished. The CD volume is dropped
 * to 0 first. The opcode is skipped when XA audio is already playing, starting or
 * stopping. Four bytes. */
void open_opcode_start_xa_audio(s16* command) {
    u32 flags = g_open_script_state.dispatch.flags;

    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (flags & OPEN_SCRIPT_DISPATCH_XA_STARTING) {
            g_open_script_state.dispatch.flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
            return;
        }
        g_open_script_state.dispatch.flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
        g_open_script_state.dispatch.cursor.byte_offset += 4;
        return;
    }
    if (flags
        & (OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING)) {
        g_open_script_state.dispatch.cursor.byte_offset += 4;
        return;
    }
    g_open_script_state.control_parameter = command[1];
    g_open_script_state.control_state = 0;
    g_open_script_state.dispatch.flags = (flags
                                             & ~(OPEN_SCRIPT_DISPATCH_CONTINUE | OPEN_SCRIPT_DISPATCH_XA_PLAYING
                                                 | OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING))
        | (OPEN_SCRIPT_DISPATCH_WAITING | OPEN_SCRIPT_DISPATCH_XA_STARTING);
    open_sound_set_cd_mix_and_volume(0, 1);
}
