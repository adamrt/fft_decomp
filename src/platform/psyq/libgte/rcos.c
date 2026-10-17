#include "psx/libgte.h"

/* main 0x8001bc28–0x8001bcc8. */
s32 rcos(s32 angle) {
    s32 result;
    if (angle < 0)
        angle = -angle;
    angle &= (ONE - 1);
    if (angle <= (ONE / 2)) {
        if (angle <= (ONE / 4))
            result = g_psyq_gte_sine_quarter_table[(ONE / 4) - angle];
        else
            result = -g_psyq_gte_sine_quarter_table[angle - (ONE / 4)];
    } else {
        if (angle > (3 * ONE / 4))
            result = g_psyq_gte_sine_quarter_table[angle - (3 * ONE / 4)];
        else
            result = -g_psyq_gte_sine_quarter_table[(3 * ONE / 4) - angle];
    }
    return result;
}
