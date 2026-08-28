#include "fft/world.h"

/* Store sorted item IDs in a byte list.
 *
 * The halfword input ends at -1; the byte output ends at 0xff.
 */
void world_item_finalize_sorted_list(s32 list_index, s16* sorted_ids) {
    s32 count;
    s16* source;
    u8* list;
    u8* destination;
    u8 item_id;
    s32 terminator;

    source = sorted_ids;
    list = g_world_item_sorted_lists[list_index];
    count = 0;
    if (*source != -1) {
        terminator = -1;
        destination = list;
        do {
            item_id = *source;
            source++;
            count++;
            *destination = item_id;
            destination++;
        } while (*source != terminator);
    }
    list[count] = ITEM_ID_NONE;
}
