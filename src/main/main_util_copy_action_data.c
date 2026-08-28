#include "psx/types.h"

void main_util_copy_action_data(const u8* source, u8* destination) {
    u32 index = 0;

    do {
        *destination++ = *source++;
        index++;
    } while (index < 20);
}
