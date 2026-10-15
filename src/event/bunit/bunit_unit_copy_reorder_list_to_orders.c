#include "fft/event_bunit.h"

/* bunit_orders is the first (12-byte, -1 terminated) list of the item type
 * order tables at 0x80057b20 (`orders.order_0` in main). */
/* Copy the low byte of each 16-bit entry into bunit_orders until the
 * terminator (-1) has been copied. The first parameter is unused. */
void bunit_unit_copy_reorder_list_to_orders(s32 unused, const s16* entries) {
    s32 index = 0;
    s8 value = *entries;

    g_main_item_type_order_tables.order_0[index] = value;
    while (value != -1) {
        entries++;
        value = *entries;
        index += 1;
        g_main_item_type_order_tables.order_0[index] = value;
    }
}
