#include "psx/types.h"

/* Widen `count` bytes at `src` into halfwords at `dst`. */
void battle_menu_widen_bytes_to_halfwords(s16* dst, u8* src, s32 count) {
    s32 i;
    char unused[8]; /* unreferenced; sizes the target's 8-byte frame */

    i = 0;
    if (count > 0) {
        do {
            *dst = *src++;
            i += 1;
            dst += 1;
        } while (i < count);
    }
}
