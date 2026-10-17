#include "psx/libapi.h"
s32 StopRCnt(u32 spec) {
    s32 counter = spec & PSYQ_RCNT_SPEC_MASK;
    g_psyq_api_interrupt_controller->mask &= ~g_psyq_api_root_counter_irq_masks[counter];
    return 1;
}
