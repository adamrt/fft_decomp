#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opcode 8: fade the CD audio in at mix level 0x7a over command[1] * 4 ticks.
 * Four bytes.
 *
 * `command` is not const, matching the script handler type: GCC 2.7.2 marks a
 * load through a const pointer unchanging and would schedule it above the
 * prologue's ra save. */
void open_opcode_start_sound_fade_in(s16* command) {
    open_sound_set_cd_mix_and_volume(0x7a, command[1] * 4);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}
