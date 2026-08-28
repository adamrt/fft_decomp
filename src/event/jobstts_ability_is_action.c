#include "fft/data.h"
#include "fft/jobstts.h"
#include "psx/types.h"

s32 jobstts_ability_is_action(void) {
    return g_jobstts_ability_type == ABILITY_TYPE_DEFAULT;
}
