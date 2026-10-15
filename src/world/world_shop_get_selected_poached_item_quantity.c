#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Query the selected poached-stock entry with a 0 quantity adjustment.
 *
 * An empty menu returns 0. */
s32 world_shop_get_selected_poached_item_quantity(s32 unused_row) {
    if (g_menu_list_entry_count != 0)
        return world_shop_adjust_poached_item_quantity(g_item_list_entries[g_menu_list_selected_index], 0);
    return 0;
}
