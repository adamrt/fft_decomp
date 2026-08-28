#include "fft/world.h"
#include "psx/libc.h"

/* Filter and reorder halfword entries by the selected inventory list.
 *
 * Match each item by its low byte while preserving the full input halfword.
 */
s32 world_item_filter_sorted_entries(s32 list_index, world_item_list_entry_t* entries) {
    s16 filtered[140];
    u8* list;
    s32 count;
    s32 i;
    s32 retained;
    s32 j;

    list = g_world_item_sorted_lists[list_index];
    count = 0;
    while (list[count] != ITEM_ID_NONE) {
        count++;
    }
    retained = 0;
    for (i = 0; i < count; i++) {
        for (j = 0; entries[j].value != -1; j++) {
            if (list[i] == entries[j].bytes.item_id) {
                filtered[retained] = entries[j].value;
                retained++;
                break;
            }
        }
    }
    filtered[retained] = -1;
    bcopy(filtered, entries, (retained + 1) * sizeof(s16));
    return retained;
}
