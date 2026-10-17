#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d3cc–0x8001d3e4: hardware leading-sign-bit count. */
s32 Lzc(s32 value) {
    s32 count;
    PSYQ_GTE_LZCS(value, count);
    return count;
}
