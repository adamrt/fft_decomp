#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_job_is_current_mastered(s32 unused_row) {
    return g_job_current_mastered;
}
