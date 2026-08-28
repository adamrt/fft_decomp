#include "psx/types.h"

void battle_script_clamp_s32_to_range(s32* value, s32 minimum, s32 maximum) {
    s32 current;

    current = *value;
    if (current < minimum) {
        *value = minimum;
        return;
    }
    if (maximum < current) {
        *value = maximum;
    }
}
