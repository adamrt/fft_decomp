#include "psx/types.h"

s32 battle_menu_copy_ff_terminated_bytes_to_halfwords(s16* dst, u8* src) {
    s32 count;

    count = 0;
    for (;;) {
        if (*src == 0xFF) {
            return count;
        }
        *dst = *src;
        src += 1;
        count += 1;
        dst += 1;
    }
}
