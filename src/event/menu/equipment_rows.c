/* Frame-wait idle and the equipment menu row item counts and icon sources. */

#include "fft/data_item.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Thread body that never ends, waiting one frame at a time; it holds a thread slot until another
 * thread stops it. */
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
    s32 result = item_get_total_count(entry & ITEM_ENTRY_ID_MASK);

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
    s32 result = item_get_equipped_count(entry & ITEM_ENTRY_ID_MASK);

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

/* Menu callback: the icon descriptor of the item in list row `row`, built in the shared
 * g_item_graphic_descriptor. The row's 16-bit entry holds the item id in its low byte. */
menu_icon_source_t* menu_get_entry_item_icon_source(s32 row) {
    menu_build_item_icon_source(&g_item_graphic_descriptor, g_item_list_entries[row] & 0xff);
    return &g_item_graphic_descriptor;
}
