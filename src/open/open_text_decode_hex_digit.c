#include "psx/types.h"

s32 open_text_decode_hex_digit(s32 character) {
    s32 value;

    value = character - '0';
    if ((u32)value < 10) {
        return value;
    }

    if ((u32)(character - 'A') < 6) {
        return character - ('A' - 10);
    }

    if ((u32)(character - 'a') < 6) {
        return character - ('a' - 10);
    }

    return 0;
}
