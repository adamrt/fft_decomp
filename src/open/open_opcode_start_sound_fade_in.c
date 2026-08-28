#include "fft/open.h"

/* Starts the sound fade-in for command[1] and advances past the command.
 *
 * `command` is not const, matching the script handler type: GCC 2.7.2 marks a
 * load through a const pointer unchanging and would schedule it above the
 * prologue's ra save. */
void open_opcode_start_sound_fade_in(s16* command) {
    open_sound_set_type_and_volume(0x7a, command[1] * 4);
    g_open_script_state.dispatch.byte_offset += 4;
}
