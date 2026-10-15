#include "fft/menu.h"

/* Fill a 16-byte text field with terminators. */
void text_fill_field_with_terminators(u8* text) {
    u8 value = TEXT_END_WAIT_FOR_CONFIRM;
    s32 index;

    for (index = 15; index >= 0; index--) {
        *text++ = value;
    }
}
