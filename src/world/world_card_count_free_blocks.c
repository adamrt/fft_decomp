#include "psx/etc.h"
#include "psx/types.h"

/*
 * Count the free blocks after rounding each directory entry up to 8 KiB.
 *
 * The card has 15 usable blocks. Indexed traversal and the early full-card
 * return preserve the target's loop scheduling and common return.
 */
s32 world_card_count_free_blocks(DIRENTRY* entries, s32 count) {
    s32 used = 0;
    s32 i;

    for (i = 0; i < count; i++) {
        used += entries[i].size >> 13;
        used += (entries[i].size & 0x1fff) != 0;
    }
    if (used >= 16) {
        return 0;
    }
    return 15 - used;
}
