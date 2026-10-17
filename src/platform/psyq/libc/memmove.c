#include "psx/libc.h"
/* Retail returns the forward-copy cursor, but the original destination for backward copies. */
void* memmove(void* destination, const void* source, s32 count) {
    u8* to = destination;
    const u8* from = source;
    if (to >= from) {
        if (count-- > 0) {
            /* Integer-address addition preserves the native ADDU operand order. */
            u8* end_to = (u8*)(count + (u32)to);
            const u8* end_from = (const u8*)(count + (u32)from);
            do {
                *end_to-- = *end_from--;
            } while (count-- > 0);
        }
    } else {
        while (count-- > 0) {
            *to++ = *from++;
        }
    }
    return to;
}
