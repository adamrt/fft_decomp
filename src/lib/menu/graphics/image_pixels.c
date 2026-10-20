/* Copy nonempty image data and mirror packed 4bpp rectangles through nibble accessors. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Copy a nonempty byte sequence from source to destination.
 *
 * The target performs the first byte copy before testing the decremented
 * count, so callers must provide a positive byte count. */
void mem_copy_bytes_nonempty(void* destination, const void* source, s32 count) {
    /* Pin: unpinned, GCC copies the byte through $v0 instead of $a3. */
    register u8 value __asm__("$7");
    u8* destination_bytes = destination;
    const u8* source_bytes = source;

    /* The retail loop uses trapping addi updates, a nop after the lbu and an
     * unfilled bnez delay slot. GCC otherwise emits addiu and fills both. */
    do {
        value = *source_bytes;
        *destination_bytes = value;
        __asm__ volatile("addi %0,%0,1" : "=r"(destination_bytes) : "0"(destination_bytes));
        __asm__ volatile("addi %0,%0,1" : "=r"(source_bytes) : "0"(source_bytes));
        __asm__ volatile("addi %0,%0,-1" : "=r"(count) : "0"(count));
    } while (count);
}

/* Nibble accessors for a 4bpp image whose rows are `stride` pixels wide.
 * WORLD inlines them into the mirror; BATTLE calls them. The target evaluates
 * the argument expressions before choosing the mask and multiplies
 * afterwards. */
static
#ifdef FFT_WORLD
    inline
#endif
    s32 gfx_read_packed_nibble(const u8* pixels, s32 x, s32 y, s32 stride) {
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

/* The parity test lands in the mask's own register, which is dead there
 * because `pixels` has already been copied out of it, so the parity is
 * assigned to `mask` rather than tested inline. */
static
#ifdef FFT_WORLD
    inline
#endif
    void gfx_write_packed_nibble(u8* pixels, s32 x, s32 y, s32 stride, s32 value) {
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
 * both ends of each row toward the middle. BATTLE calls the nibble helpers
 * from plain `for` loops; WORLD inlines them, and its loop is an explicitly
 * guarded do/while because of these placement facts:
 *  - `right = 1` is emitted between the loop's entry test and the loop top,
 *    which is only reachable when the initialization follows the guard's jump;
 *    with a `for`, the inits precede the duplicated exit test and the `li 1`
 *    is instead hoisted out of the row loop (a partial movable, with a
 *    compensating copy in the row loop's latch).
 *  - the guard is spelled `if (col < rect->w / 2)` after `col = 0;` rather
 *    than `if (rect->w / 2 > 0)`, so that `col = 0` precedes the guard branch
 *    (reorg puts it in the branch's delay slot) and `col` keeps the extra
 *    reference that makes it outrank `row` in the allocation order.
 *  - `right++` before `col++` puts `col++` in the reload's load shadow and
 *    `right++` in the loop branch's delay slot. */
#ifdef FFT_BATTLE
void gfx_mirror_4bpp_rect(u8* pixels, const glyph_blit_t* rect) {
    s32 row;
    s32 col;
    s32 offset;
    s32 left_value;
    s32 right_value;

    for (row = 0; row < rect->rect.h; row++) {
        for (col = 0; col < rect->rect.w / 2; col++) {
            left_value = gfx_read_packed_nibble(pixels, rect->rect.x + col, rect->rect.y + row, rect->stride);
            offset = 1;
            right_value = gfx_read_packed_nibble(
                pixels, rect->rect.x + rect->rect.w - (col + offset), rect->rect.y + row, rect->stride);
            gfx_write_packed_nibble(pixels, rect->rect.x + col, rect->rect.y + row, rect->stride, right_value);
            gfx_write_packed_nibble(
                pixels, rect->rect.x + rect->rect.w - (col + offset), rect->rect.y + row, rect->stride, left_value);
        }
    }
}
#endif
#ifdef FFT_WORLD
/* WORLD's body of the same mirror, with the guarded do/while loop described above. */
void gfx_mirror_4bpp_rect(u8* pixels, const glyph_blit_t* rect) {
    s32 row;
    s32 col;
    s32 right;
    s32 left_value;
    s32 right_value;

    for (row = 0; row < rect->rect.h; row++) {
        col = 0;
        if (col < rect->rect.w / 2) {
            right = 1;
            do {
                left_value = gfx_read_packed_nibble(pixels, rect->rect.x + col, rect->rect.y + row, rect->stride);
                right_value = gfx_read_packed_nibble(
                    pixels, rect->rect.x + rect->rect.w - right, rect->rect.y + row, rect->stride);
                gfx_write_packed_nibble(pixels, rect->rect.x + col, rect->rect.y + row, rect->stride, right_value);
                gfx_write_packed_nibble(
                    pixels, rect->rect.x + rect->rect.w - right, rect->rect.y + row, rect->stride, left_value);
                right++;
                col++;
            } while (col < rect->rect.w / 2);
        }
    }
}
#endif
