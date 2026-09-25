#include "fft/world.h"
#include "psx/types.h"

world_item_icon_source_t* world_menu_get_entry_item_icon_source(s32 index) {
    world_item_build_icon_source(
        &g_world_menu_entry_item_icon_source, ((world_item_list_entry_t*)g_world_menu_entry_ids)[index].bytes.item_id);
    return &g_world_menu_entry_item_icon_source;
}
