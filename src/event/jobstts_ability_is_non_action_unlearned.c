#include "fft/data.h"
#include "fft/jobstts.h"
#include "psx/types.h"

s32 jobstts_ability_is_non_action_unlearned(s32 index) {
    s32 result = 0;

    if (g_jobstts_ability_type != ABILITY_TYPE_DEFAULT) {
        result = (((u16*)g_jobstts_ability_entries)[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) != 0;
    }
    return result;
}
