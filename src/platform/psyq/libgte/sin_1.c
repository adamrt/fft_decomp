#include "psx/libgte.h"

/* main 0x8001bb98–0x8001bc28: quadrant lookup. */
s32 sin_1(s32 angle) {
    s32 result;
    if (angle <= (ONE / 2)) {
        if (angle <= (ONE / 4))
            result = g_psyq_gte_sine_quarter_table[angle];
        else
            result = g_psyq_gte_sine_quarter_table[(ONE / 2) - angle];
    } else {
        if (angle > (3 * ONE / 4))
            result = -g_psyq_gte_sine_quarter_table[ONE - angle];
        else
            result = -g_psyq_gte_sine_quarter_table[angle - (ONE / 2)];
    }
    return result;
}
