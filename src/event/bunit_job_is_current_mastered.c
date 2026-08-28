#include "fft/bunit.h"
#include "psx/types.h"

s32 bunit_job_is_current_mastered(void) {
    return g_bunit_job_current_mastered;
}
