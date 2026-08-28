#include "fft/world.h"
#include "psx/types.h"

/* Builds the 12x12 item-type icon texture source for one shop/inventory menu
 * entry and returns the shared record. */
world_item_icon_source_t* world_item_build_type_icon_source(s32 entry_index) {
    world_item_icon_source_t* source = &g_world_item_type_icon_source;

    /* The target uses v0 without the u8 return's mask. */
    world_item_get_type_icon_rect(((s32 (*)(s32))world_item_get_type)(g_world_menu_entry_ids[entry_index]), source);
    source->clut = g_world_menu_palette_clut;
    source->tpage = g_world_menu_icon_tpage;
    return source;
}
