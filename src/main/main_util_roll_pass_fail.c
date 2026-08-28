#include "psx/types.h"

s32 main_util_roll_pass_fail(s32 maximum, s32 threshold) {
    s32 roll = rand() * maximum / 0x8000;

    return roll >= threshold;
}
