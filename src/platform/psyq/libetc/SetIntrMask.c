#include "psx/libetc.h"

s32 SetIntrMask(s32 mask) {
    s32 previous = *g_psyq_etc_irq_mask;
    *g_psyq_etc_irq_mask = mask;
    return previous;
}
