#include "psx/types.h"

/* Target 0x800ffc28. Copies a 16-bit pattern row: the two leading and two
 * trailing entries verbatim, then the four interior entries repeated every
 * fourth slot across the row. */
void world_menu_fill_pattern_row(u16* source, u16* dest, s32 count, s32 offset) {
    s32 i;
    u16* slot;
    u16 value;

    dest[0] = source[offset + 0];
    dest[1] = source[offset + 1];
    dest[count - 4] = source[offset + 6];
    dest[count - 3] = source[offset + 7];

    value = source[offset + 2];
    slot = dest + 2;
    for (i = 0; i < count - 6; i += 4, slot += 4) {
        *slot = value;
    }
    value = source[offset + 3];
    slot = dest + 3;
    for (i = 0; i < count - 7; i += 4, slot += 4) {
        *slot = value;
    }
    value = source[offset + 4];
    slot = dest + 4;
    for (i = 0; i < count - 8; i += 4, slot += 4) {
        *slot = value;
    }
    value = source[offset + 5];
    slot = dest + 5;
    for (i = 0; i < count - 9; i += 4, slot += 4) {
        *slot = value;
    }
}
