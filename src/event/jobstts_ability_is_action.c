#include "fft/event_jobstts.h"
#include "psx/types.h"

s32 jobstts_ability_is_action(void) {
    return g_jobstts_ability_type == ABILITY_TYPE_DEFAULT;
}
