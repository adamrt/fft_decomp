#include "fft/event_equip.h"
#include "fft/menu.h"

/* Build the item-type icon source for one menu row. */
menu_icon_source_t* menu_build_item_type_icon_source(s32 index) {
    menu_icon_source_t* out = &g_menu_item_type_icon_source;
    equip_item_get_type_icon_rect(item_get_type(g_item_list_entries[index]), out);
    out->clut = g_menu_palette_clut;
    out->tpage = g_menu_cursor_texture_page;
    return out;
}
