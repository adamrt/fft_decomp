#include "fft/event_bunit.h"
#include "fft/job.h"
#include "psx/types.h"

s32 job_get_current_total_jp(s32 unused_row) {
    return g_job_current_total_jp;
}
