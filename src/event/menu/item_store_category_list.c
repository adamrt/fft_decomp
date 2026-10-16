/* Storing an item list as a category list. */

#include "fft/item.h"
#include "psx/types.h"

/* Compact a -1-terminated s16 item list and append its byte terminator. */
void item_store_category_list(s32 category_index, const s16* source) {
    u8* base = g_item_category_lists[category_index];
    s32 count = 0;

    while (*source != -1) {
        base[count] = *source++;
        count++;
    }
    base[count] = ITEM_LIST_END;
}
