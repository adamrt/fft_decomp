#include "fft/event_equip.h"

/* EQUIP.OUT 001c41dc - Build the icon rect for the item in menu slot `index`. */
equip_icon_rect_t* equip_item_build_row_icon_rect(s32 index) {
    equip_icon_rect_t* out = &g_equip_item_row_icon_rect;
    equip_item_get_type_icon_rect(equip_item_get_type(g_equip_item_list_entries[index]), &out->rect);
    out->clut = g_equip_text_metric_2;
    out->tpage = g_equip_menu_cursor_texture_page;
    return out;
}
