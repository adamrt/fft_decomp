#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Wrap-around cursor step for one menu axis: the first button decrements the
 * stored cursor (wrapping to count - 1 at zero), the second increments it
 * (wrapping to zero at count - 1); the stored position is returned either way.
 *
 * Three locals per arm rather than one are what the target's code needs:
 * `value` is the copy of the loaded halfword that the arithmetic reads (the
 * target keeps the loaded value and the value it modifies in two registers,
 * and reorg fills the branch delay slot with that copy), and `wide` is the
 * sign-extended use that makes the first arm's load an `lh`; without it a
 * halfword compared only against zero is loaded with `lhu`.  `next` stays at
 * function scope because the two arms share one `sh` through cross-jumping. */
s16 world_move_menu_cursor_horizontal(u16 count, u8 index, s32 buttons) {
    s16 cur;
    s32 wide;
    s16 value;
    s16 cur2;
    s16 value2;
    s32 slot;
    s16* base;
    s16* cursor;
    s16 next;

    if (buttons & PSX_PAD_LEFT) {
        slot = index;
        base = g_world_menu_cursor_positions;
        cursor = base + slot;
        cur = *cursor;
        wide = cur;
        value = cur;
        if (wide == 0) {
            next = count - 1;
        } else {
            next = value - 1;
        }
        *cursor = next;
    } else if (buttons & PSX_PAD_RIGHT) {
        slot = index;
        base = g_world_menu_cursor_positions;
        cursor = base + slot;
        cur2 = *cursor;
        value2 = cur2;
        if (cur2 < count - 1) {
            next = value2 + 1;
        } else {
            next = 0;
        }
        *cursor = next;
    }
    next = g_world_menu_cursor_positions[index];
    return next;
}
