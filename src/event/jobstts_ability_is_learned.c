#include "fft/data.h"
#include "fft/jobstts.h"
#include "psx/types.h"

s32 jobstts_ability_is_learned(s32 index) {
    return ((((u16*)g_jobstts_ability_entries)[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) ^ 1) & 1;
}
