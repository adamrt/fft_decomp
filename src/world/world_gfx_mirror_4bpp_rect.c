#include "fft/world.h"
#include "psx/types.h"

/* The loop is an explicitly guarded do/while rather than a `for` because of
 * these placement facts:
 *  - `right = 1` is emitted between the loop's entry test and the loop top,
 *    which is only reachable when the initialisation follows the guard's jump;
 *    with a `for`, the inits precede the duplicated exit test and the `li 1`
 *    is instead hoisted out of the row loop (a partial movable, with a
 *    compensating copy in the row loop's latch).
 *  - the guard is spelled `if (col < rect->w / 2)` after `col = 0;` rather
 *    than `if (rect->w / 2 > 0)`, so that `col = 0` precedes the guard branch
 *    (reorg puts it in the branch's delay slot) and `col` keeps the extra
 *    reference that makes it outrank `row` in the allocation order.
 *  - `right++` before `col++` puts `col++` in the reload's load shadow and
 *    `right++` in the loop branch's delay slot.
 */

/* Nibble accessors for a 4bpp image whose rows are `stride` pixels wide.
 * The target evaluates the argument expressions before choosing the mask
 * and multiplies afterwards, which is the shape of an inlined helper. */
static inline s32 world_get_4bpp_pixel(u8* pixels, s32 x, s32 y, s32 stride) {
    s32 mask;
    s32 value;

    mask = x & 1;
    if (mask != 0) {
        mask = 0xF0;
    } else {
        mask = 0xF;
    }
    value = pixels[(y * stride + x) >> 1] & mask;
    if (mask == 0xF0) {
        value >>= 4;
    }
    return value;
}

static inline void world_set_4bpp_pixel(u8* pixels, s32 x, s32 y, s32 stride, s32 value) {
    s32 mask;
    u8* byte;

    mask = x & 1;
    if (mask != 0) {
        mask = 0xF0;
    } else {
        mask = 0xF;
    }
    if (mask == 0xF0) {
        value <<= 4;
    }
    byte = &pixels[(y * stride + x) >> 1];
    *byte = (*byte & ~mask) | value;
}

/* Mirrors a rectangle of a 4bpp image horizontally, swapping pixels from
 * both ends of each row toward the middle. */
void world_gfx_mirror_4bpp_rect(u8* pixels, world_gfx_4bpp_rect_t* rect) {
    s32 row;
    s32 col;
    s32 right;
    s32 left_value;
    s32 right_value;

    for (row = 0; row < rect->h; row++) {
        col = 0;
        if (col < rect->w / 2) {
            right = 1;
            do {
                left_value = world_get_4bpp_pixel(pixels, rect->x + col, rect->y + row, rect->stride);
                right_value = world_get_4bpp_pixel(pixels, rect->x + rect->w - right, rect->y + row, rect->stride);
                world_set_4bpp_pixel(pixels, rect->x + col, rect->y + row, rect->stride, right_value);
                world_set_4bpp_pixel(pixels, rect->x + rect->w - right, rect->y + row, rect->stride, left_value);
                right++;
                col++;
            } while (col < rect->w / 2);
        }
    }
}
