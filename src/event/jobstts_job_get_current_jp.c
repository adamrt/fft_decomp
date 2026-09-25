#include "fft/event_jobstts.h"
#include "psx/types.h"

u32 jobstts_job_get_current_jp(void) {
    return g_jobstts_job_current_jp;
}
