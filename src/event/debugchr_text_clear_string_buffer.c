#include "psx/types.h"

void debugchr_text_clear_string_buffer(u8* output) {
    s32 i;
    u8 value;

    value = 0xfe;
    for (i = 15; i >= 0; i--) {
        *output++ = value;
    }
}
