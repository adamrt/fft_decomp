#include "psx/libetc.h"

s32 GetIntrMask(void) {
    return *g_psyq_etc_irq_mask;
}
