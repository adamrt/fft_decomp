#include "psx/libgs.h"
#include "psx/types.h"

/* The ordering-table insertion helper shared by the sprite, line, box and
 * polygon sorters; its diagnostic at 0x800e0000 reads "ps_sort_sprite,bg: z
 * resolution overflow\n". It links the packet at `tag` into the OT bucket for
 * `z` and returns the address just past the packet, which the caller stores
 * as the new packet cursor.
 */

PACKET* psyq_gs_sort_sprite_bg(u32* tag, GsOT* ot, u16 z, s32 len) {
    u8 packet_length = len;
    GsOT_TAG* slot;
    s32 idx;

    idx = z - ot->offset;
    if (idx < 0) {
        printf(g_psyq_gs_sort_sprite_overflow_message);
    }
    slot = ot->org + idx;
    /* Whole GPU tag transfers match; bitfield updates add masking and read-modify-write instructions. */
    *tag = *(u32*)slot;
    setlen(tag, packet_length);
    *(u32*)slot = (u32)tag;
    slot->num = 0;
    return (PACKET*)(tag + (packet_length + 1));
}
