#include "psx/types.h"

void battle_text_fill_field_with_terminators(u8* text) {
    s32 index;
    u8* cursor = text;

    /* GCC reverses this count-up loop; this form preserves the target's
     * terminator-before-counter initialization order. */
    for (index = 0; index < 16; index++) {
        *cursor++ = 0xfe;
    }
}
