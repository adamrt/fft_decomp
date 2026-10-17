#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d5a8–0x8001d5cc. */
long NormalClip(long a, long b, long c) {
    long result;
    PSYQ_GTE_NORMALCLIP(a, b, c, result);
    return result;
}
