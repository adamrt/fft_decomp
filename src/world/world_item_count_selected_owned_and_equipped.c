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

/* Returns the owned-and-equipped count for the selected item. */
s32 world_item_count_selected_owned_and_equipped(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_get_total_count_variant(g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}
