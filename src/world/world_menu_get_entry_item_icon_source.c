#include "fft/world.h"
#include "psx/types.h"

u8* world_menu_get_entry_item_icon_source(s32 index) {
    world_item_build_icon_source(
        (world_item_icon_source_t*)g_world_menu_entry_item_icon_source, ((u8*)g_world_menu_entry_ids)[index * 2]);
    return g_world_menu_entry_item_icon_source;
}
