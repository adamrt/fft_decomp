#include "fft/event_jobstts.h"
#include "psx/types.h"

s32 jobstts_job_get_current_jp(s32 unused_row) {
    return g_jobstts_job_current_jp;
}
