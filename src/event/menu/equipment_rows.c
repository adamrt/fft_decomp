/* Frame-wait idle and the equipment menu row item counts and icon sources. */

#include "fft/event_equip.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/thread.h"
#include "psx/types.h"

void thread_wait_forever_by_frames(void) {
    for (;;) {
        thread_wait_frames(1);
    }
}

/* Get the total count (inventory plus equipped) of the item in an equipment-menu row.
 *
 * MENU_NUMBER_VALUE_DISABLED is set when the unit cannot equip the item. */
s32 menu_get_entry_item_count(s32 row) {
    u16 entry = g_item_list_entries[row];
    s32 result = item_get_total_count_variant(entry & ITEM_ENTRY_ID_MASK);

    if (g_item_list_entries[row] & ITEM_ENTRY_UNEQUIPPABLE) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}

/* Get the equipped count of the item in an equipment-menu row.
 *
 * MENU_NUMBER_VALUE_DISABLED is set when the unit cannot equip the item. */
s32 menu_get_entry_item_equipped_count(s32 row) {
    u16 entry = g_item_list_entries[row];
    s32 result = item_get_equipped_count_variant(entry & ITEM_ENTRY_ID_MASK);
    if (g_item_list_entries[row] & ITEM_ENTRY_UNEQUIPPABLE) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}

/* Build the item-type icon source for one menu row. */
menu_icon_source_t* menu_build_item_type_icon_source(s32 index) {
    menu_icon_source_t* out = &g_menu_item_type_icon_source;
    item_get_type_icon_rect(item_get_type(g_item_list_entries[index]), out);
    out->clut = g_menu_palette_clut;
    out->tpage = g_menu_cursor_texture_page;
    return out;
}

/* Menu callback: the icon descriptor of the item in list row `row`, built in
 * a shared record. */
menu_icon_source_t* menu_get_entry_item_icon_source(s32 row) {
    /* item list rows are 16-bit entries whose low byte is the item id
     * (see menu_build_item_type_icon_source / menu_get_entry_item_equipped_count). */
    /* Descriptor of six 16-bit values filled by menu_build_item_icon_source. */
    menu_build_item_icon_source(&g_item_graphic_descriptor, g_item_list_entries[row] & 0xff);
    return &g_item_graphic_descriptor;
}
