#include "fft/item.h"
#include "psx/types.h"

void menu_load_sort_key_list(s32 list_index, s16* destination) {
    s8* source;
    s32 i;

    source = g_item_type_order_lists[list_index];
    i = 0;
    *destination = source[i];
    while (*destination != -1) {
        i++;
        destination++;
        *destination = source[i];
    }
}
