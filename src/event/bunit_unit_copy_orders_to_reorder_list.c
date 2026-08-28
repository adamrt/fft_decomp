#include "fft/bunit.h"
#include "fft/card.h"
#include "fft/main_runtime.h"

/* Copies the first signed-byte order list into a -1-terminated s16 list. */
void bunit_unit_copy_orders_to_reorder_list(s32 unused, s16* entries) {
    s32 i;

    for (i = 0; (*entries = g_main_item_type_order_tables.order_0[i]) != -1; i++) {
        entries++;
    }
}
