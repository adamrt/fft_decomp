#include "psx/libapi.h"
s32 ResetRCnt(u32 spec) {
    s32 counter = spec & PSYQ_RCNT_SPEC_MASK;
    if (counter >= PSYQ_RCNT_HARDWARE_COUNT) {
        return 0;
    }
    g_psyq_api_root_counters[counter].count = 0;
    return 1;
}
