#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 2: set the text scroll speed to command[1] (1/256 pixel per frame,
 * negative for upward) and zero the fraction and the scrolled-distance counter.
 * Four bytes. */
void open_opcode_set_timing_step(s16* entry) {
    const s16* command = entry;
    s32 value = command[1];
    s32 next = g_open_script_byte_offset + 4;

    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = value;
    g_open_script_byte_offset = next;
}
