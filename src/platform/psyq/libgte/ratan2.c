#include "psx/libgte.h"

/* main 0x8001d8e8–0x8001da68. */
long ratan2(long y, long x) {
    long negative_x = 0;
    long negative_y = 0;
    long result;
    if (x < 0) {
        negative_x = 1;
        x = -x;
    }
    if (y < 0) {
        negative_y = 1;
        y = -y;
    }
    if (!x && !y)
        return 0;
    if (y < x) {
        if (y & 0x7fe00000)
            y /= x >> 10;
        else
            y = (y << 10) / x;
        result = g_psyq_gte_atan_ratio_table[y];
    } else {
        if (x & 0x7fe00000)
            y = x / (y >> 10);
        else
            y = (x << 10) / y;
        result = ONE / 4 - g_psyq_gte_atan_ratio_table[y];
    }
    if (negative_x)
        result = ONE / 2 - result;
    if (negative_y)
        result = -result;
    return result;
}
