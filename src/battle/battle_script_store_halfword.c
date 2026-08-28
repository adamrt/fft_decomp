#include "psx/types.h"

void battle_script_store_halfword(u8* dst, s16 value) {
    dst[0] = value & 0xff;
    dst[1] = value >> 8;
}
