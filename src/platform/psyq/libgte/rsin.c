#include "psx/libgte.h"

/* main 0x8001bb5c–0x8001bb98. */
s32 rsin(s32 angle) {
    s32 result;
    if (angle >= 0)
        result = sin_1(angle & (ONE - 1));
    else
        result = -sin_1(-angle & (ONE - 1));
    return result;
}
