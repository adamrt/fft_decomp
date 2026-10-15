#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_job_get_current_jp(s32 unused_row) {
    return g_bunit_job_current_jp;
}
