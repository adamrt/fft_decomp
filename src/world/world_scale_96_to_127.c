#include "psx/types.h"

s32 world_scale_96_to_127(s32 value) {
    if (value == 0) {
        return 0;
    }
    if (value >= 0x60) {
        value = 0x7F;
    } else {
        value = (value * 0x7F) / 96;
    }
    return value;
}
