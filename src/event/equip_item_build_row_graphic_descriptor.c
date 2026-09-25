#include "fft/equip.h"
#include "psx/types.h"

battle_menu_status_panel_graphic_descriptor_t* equip_item_build_row_graphic_descriptor(s32 row) {
    /* item_t list rows are halfword entries whose low byte is the item id
     * (see equip_item_build_row_icon_rect / equip_item_get_available_with_equip_flag). */
    /* 6-halfword descriptor filled by equip_gfx_build_item_graphic_descriptor. */
    /* The target also passes the row's item id in a1 to this one-argument callee. */
    ((void (*)(battle_menu_status_panel_graphic_descriptor_t*, s32))equip_gfx_build_item_graphic_descriptor)(
        &g_equip_item_graphic_descriptor, ((u8*)g_equip_item_list_entries)[row * 2]);
    return &g_equip_item_graphic_descriptor;
}
