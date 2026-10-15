#include "fft/main.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Copy the formation graphic entry of a generic human job. Jobs below 0x35
 * are looked up in a table; of the rest, Bard and Dancer have fixed entries
 * (0x3a, 0x3b), while Mime uses 0x3c and the others job * 2 - 0x7c, both
 * plus one for a female unit. */
void world_formation_get_generic_human_graphic_entry(s32 job_id, s16 is_female, texture_source_t* destination) {
    s16 id = job_id;
    s16 idx;

    if (id < JOB_ID_CHEMIST_35) {
        idx = g_world_formation_generic_human_graphic_index_table[id];
    } else if (id == JOB_ID_DANCER) {
        idx = 0x3B;
    } else if (id == JOB_ID_BARD) {
        idx = 0x3A;
    } else {
        if (id == JOB_ID_MIME) {
            idx = 0x3C;
        } else {
            idx = (job_id * 2) - 0x7C;
        }
        if (is_female != 0) {
            idx += 1;
        }
    }
    bcopy(&g_world_formation_graphic_entries[idx], destination, 0xC);
}
