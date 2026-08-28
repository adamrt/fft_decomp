#include "fft/main_runtime.h"

/* Returns -1 for no current encounter and NULL while its file is loading. */
entd_encounter_t* main_entd_get_encounter(void) {
    int* event_id = &g_main_entd_current_event_id;
    int id;
    /* Pins required: unpinned, id moves to $a0 and the index and result swap $v0/$v1. */
    register int index __asm__("$2");

    if (*event_id == 0) {
        return (entd_encounter_t*)-1;
    }
    if (main_file_is_still_loading() != 0) {
        return 0;
    }
    id = *event_id;
    if (id < 0x80) {
        return &g_main_entd_set->encounters[id];
    }
    if (id < 0x100) {
        index = id - 0x80;
    } else {
        index = id < 0x180;
        if (index) {
            /* Keep the predicate live until this arm begins, preventing the
             * subtraction from replacing it in the conditional delay slot. */
            __asm__("" : : "r"(index));
            index = id - 0x100;
        } else {
            index = id - 0x180;
        }
    }
    {
        register entd_encounter_t* result __asm__("$2") = &g_main_entd_set->encounters[index];
        /* Keep this return separate from the first-file return. Binding
         * the result to the return register avoids an extra copy; the empty
         * use prevents cross-jumping without emitting an instruction. */
        __asm__("" : : "r"(result));
        return result;
    }
}
