#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

void world_formation_get_generic_human_graphic_entry(
    s32 job_id, s16 is_female, world_formation_graphic_entry_t* destination) {
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
