#include "fft/world.h"
#include "psx/gs.h"
#include "psx/types.h"

/* libgs `ps_sort_sprite,bg`: the shared ordering-table insertion helper for
 * GsSortSprite and GsSortBg. Its name comes from the diagnostic string at
 * 0x800e0000, "ps_sort_sprite,bg: z resolution overflow\n". It links the
 * packet at `tag` into the OT bucket for `z` and returns the address just
 * past the packet, which the caller stores as the new packet cursor.
 */

extern void printf(const char* msg, ...);

u32 world_ps_sort_sprite_bg(u32* tag, GsOT* ot, s32 z, s32 len) {
    u32* slot;
    s32 idx;
    u32 next;

    z = z & 0xFFFF;
    idx = z - ot->offset;
    if (idx < 0) {
        printf(g_world_gfx_sort_sprite_overflow_message);
    }
    slot = ot->org + idx;
    next = (len & 0xFF) * 4 + 4;
    /* Keeps sched2 from sinking `next` (dead until the return) past the packet stores. */
    __asm__ volatile("");
    *tag = *slot;
    /* Puts `slot` in $v1 and the loaded word in $a0 rather than the reverse. */
    __asm__("" : : "r"(tag));
    ((u8*)tag)[3] = (u8)len;
    *slot = (u32)tag;
    ((u8*)slot)[3] = 0;
    return (u32)tag + next;
}
