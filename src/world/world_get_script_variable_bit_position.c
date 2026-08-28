#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Maps a script variable id to its bit position within its storage word.
 * Ids below 0x80 have no bit slot; ids at or above 0x400 are invalid and stop
 * the calling thread. */
s32 world_get_script_variable_bit_position(s32 variable_id) {
    s32 bit_position;

    bit_position = -1;
    if (variable_id >= EVENT_SCRIPT_VAR_PACKED_BIT_FIRST) {
        if (variable_id < EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
            bit_position = variable_id & 0x1F;
        } else if (variable_id < EVENT_SCRIPT_VAR_END) {
            bit_position = (variable_id & 7) * 4;
        } else {
            world_thread_exit_current();
        }
    }
    return bit_position;
}
