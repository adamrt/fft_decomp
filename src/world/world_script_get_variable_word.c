#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Resolves a script variable id to the storage word that holds it. Ids below
 * 0x80 own a whole word; 0x80..0x35F pack 32 flags per word starting at word
 * 0x80, and 0x360..0x3FF pack eight nibbles per word starting at word 0x97.
 * Ids at or above 0x400 are invalid and stop the calling thread. */
s32* world_script_get_variable_word(s32 variable_id) {
    s32* word;

    if (variable_id < EVENT_SCRIPT_VAR_PACKED_BIT_FIRST) {
        word = &g_world_script_variables[variable_id];
    } else if (variable_id < EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
        word = &g_world_script_variables[EVENT_SCRIPT_PACKED_BIT_WORD_FIRST
            + ((variable_id - EVENT_SCRIPT_VAR_PACKED_BIT_FIRST) / 32)];
    } else if (variable_id < EVENT_SCRIPT_VAR_END) {
        word = &g_world_script_variables[EVENT_SCRIPT_PACKED_NIBBLE_WORD_FIRST
            + ((variable_id - EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) / 8)];
    } else {
        world_thread_exit_current();
    }
    return word;
}
