#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Menu callback: the icon descriptor of the item in list row `row`, built in
 * a shared record. */
menu_icon_source_t* menu_get_entry_item_icon_source(s32 row) {
    /* item list rows are 16-bit entries whose low byte is the item id
     * (see equip_item_build_type_icon_source / equip_menu_get_entry_item_equipped_count). */
    /* Descriptor of six 16-bit values filled by menu_build_item_icon_source. */
    menu_build_item_icon_source(&g_item_graphic_descriptor, g_item_list_entries[row] & 0xff);
    return &g_item_graphic_descriptor;
}
