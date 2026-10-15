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

/* Menu callback: half price of the highlighted item, or 0 for an empty list.
 * The row argument is not read. */
s32 world_item_get_selected_half_price(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count == 0) {
        result = 0;
    } else {
        result = world_item_get_half_price(g_item_list_entries[g_menu_list_selected_index]);
    }
    return result;
}
