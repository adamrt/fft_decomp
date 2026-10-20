/* Frame-wait idle and the equipment menu row item counts and icon sources. */

#include "fft/equipment.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/thread.h"
#include "psx/types.h"

static menu_icon_source_t g_menu_item_type_icon_source = { 0 };
static menu_icon_source_t g_item_graphic_descriptor = { 0 };

/* Thread body that never ends, waiting one frame at a time; it holds a thread slot until another
 * thread stops it. */
void thread_wait_forever_by_frames(void) {
    for (;;) {
        thread_frames_wait(1);
    }
}

/* Get the total count (inventory plus equipped) of the item in an equipment-menu row.
 *
 * MENU_NUMBER_VALUE_DISABLED is set when the unit cannot equip the item. */
s32 equipment_get_row_item_count(s32 row) {
    u16 entry = g_item_list_entries[row];
    s32 result = item_total_count_get(entry & ITEM_ENTRY_ID_MASK);

    if (g_item_list_entries[row] & ITEM_ENTRY_UNEQUIPPABLE) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}

/* Get the equipped count of the item in an equipment-menu row.
 *
 * MENU_NUMBER_VALUE_DISABLED is set when the unit cannot equip the item. */
s32 equipment_get_row_equipped_count(s32 row) {
    u16 entry = g_item_list_entries[row];
    s32 result = item_equipped_count_get(entry & ITEM_ENTRY_ID_MASK);

    if (g_item_list_entries[row] & ITEM_ENTRY_UNEQUIPPABLE) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}

/* Build the item-type icon source for one menu row. */
menu_icon_source_t* item_build_type_icon_source(s32 index) {
    menu_icon_source_t* out = &g_menu_item_type_icon_source;
    item_type_icon_build(item_type_get(g_item_list_entries[index]), out);
    out->clut = g_menu_palette_clut;
    out->tpage = g_menu_cursor_texture_page;
    return out;
}

/* Menu callback: the icon descriptor of the item in list row `row`, built in the shared
 * g_item_graphic_descriptor. The row's 16-bit entry holds the item id in its low byte. */
menu_icon_source_t* equipment_get_row_icon_source(s32 row) {
    item_icon_source_build(&g_item_graphic_descriptor, g_item_list_entries[row] & 0xff);
    return &g_item_graphic_descriptor;
}
