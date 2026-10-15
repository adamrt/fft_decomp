#include "fft/world.h"
#include "psx/types.h"

/* Menu callback: the icon source of the item in menu row `index`, built in a
 * shared record. */
menu_icon_source_t* world_menu_get_entry_item_icon_source(s32 index) {
    menu_build_item_icon_source(&g_world_menu_entry_item_icon_source, (u8)g_item_list_entries[index]);
    return &g_world_menu_entry_item_icon_source;
}
