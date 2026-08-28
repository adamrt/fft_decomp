#include "fft/world.h"
#include "psx/types.h"

void world_menu_set_slot_4_cursor_index(s32 index) {
    g_world_menu_default_entries[4].selected_index = index;
}
