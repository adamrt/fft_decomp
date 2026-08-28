#include "fft/world.h"

void world_menu_init_scrollable_list(const s16* values, s32 value, s32 selected, u32 state) {
    world_menu_init_scrollable_list_core(values, value, state);
    g_world_menu_scroll_offset = selected;
}
