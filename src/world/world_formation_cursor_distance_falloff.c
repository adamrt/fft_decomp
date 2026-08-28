#include "fft/world.h"
#include "psx/types.h"

/* Distance-based falloff: `base` minus the (y-stretched) distance from the
 * formation cursor to (x, y), clamped so it never drops below `floor`.
 *
 * The register pins reproduce the target's allocation: base in s0, floor in s1, the
 * origin pointer in v0 and the result in v1. */
s32 world_formation_cursor_distance_falloff(s32 x, s32 y, s32 base, s32 floor) {
    register world_menu_point_t* origin __asm__("$2");
    register s32 span __asm__("$16");
    register s32 limit __asm__("$17");
    register s32 result __asm__("$3");
    s32 dx;
    s32 dy;

    /* Keeps all three register saves ahead of the parameter copies. */
    __asm__ volatile("");
    span = base;
    limit = floor;
    /* Keeps the parameter copies ahead of the address materialisation. */
    __asm__ volatile("");
    origin = &g_world_formation_cursor_position;
    /* The target materialises the origin address once and keeps it in a
     * register across both field loads; laundering the pointer defeats cse's
     * fold of the constant into two direct lh %lo() loads. */
    __asm__("" : "=r"(origin) : "0"(origin));

    dx = origin->x - x;
    if (dx < 0) {
        dx = -dx;
    }
    /* Stops reorg cloning the mult into the bgez delay slot (a nop in the target). */
    __asm__ volatile("");
    dx = dx * dx;
    /* Keeps the mflo beside its mult instead of after the dy load. */
    __asm__ volatile("");
    dy = origin->y - y;
    if (dy < 0) {
        dy = -dy;
    }
    dy = dy * 2;
    dy = dy * dy;
    result = span - (csqrt((dy + dx) << 12) >> 12);
    if (result < limit) {
        result = limit;
    }
    return result;
}
