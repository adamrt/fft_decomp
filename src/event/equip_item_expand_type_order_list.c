#include "fft/event_equip.h"

/* Expand an item-type ordering table into the overlay's working list.
 *
 * The source uses signed bytes terminated by -1, while the destination uses
 * sign-extended s16 entries. The empty asm constraints keep each byte as
 * `lbu` plus an explicit shift pair, which combine would otherwise fold into
 * `lb`. */
void equip_item_expand_type_order_list(s32 list_index, s16* destination) {
    u8* source;
    u32 value;
    s32 sentinel;

    source = g_equip_item_type_order_lists[list_index];
    value = *source;
    /* Also orders the sentinel setup after the first byte load. */
    __asm__ volatile("" : "=r"(value), "=r"(sentinel) : "0"(value));
    sentinel = -1;
    /* Retain the target's explicit byte sign extension. */
    value = (s32)(value << 24) >> 24;
    *destination = value;
    if ((s32)value != sentinel) {
        s32 loop_sentinel;
        u8* cursor;

        loop_sentinel = -1;
        cursor = source;
        do {
            cursor++;
            value = *cursor;
            /* Keeps lbu + sll/sra instead of lb. */
            __asm__("" : "=r"(value) : "0"(value));
            destination++;
            value = (s32)(value << 24) >> 24;
            *destination = value;
        } while ((s32)value != loop_sentinel);
    }
}
