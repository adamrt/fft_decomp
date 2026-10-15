#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

job_data_t* main_job_get_data_pointer(s32 job_id) {
    if (job_id < JOB_ID_COUNT) {
        return &g_main_job_data[job_id];
    }
    return 0;
}
