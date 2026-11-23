/* Item types and the texture cells used for their equipment-menu icons. */

#include "fft/game/item.h"
#include "fft/menu/menu.h"
#include "fft/party/item_list.h"
#include "psx/types.h"

/* Return an item's type from the item data table; only the low ten bits of item_id are used. */
s32 item_get_type(s32 item_id) {
    return g_item_primary_data[item_id & 0x3ff].type;
}

/* The texture origin of each item type's 12x12 icon, read by item_list_build_type_icon. */
extern uv8_t g_item_type_icon_origins[];

/* Texture cell of an item type's 12x12 icon. */
void item_list_build_type_icon(s32 item_type, menu_icon_source_t* output) {
    output->u = g_item_type_icon_origins[item_type].u;
    output->v = g_item_type_icon_origins[item_type].v;
    output->w = 12;
    output->h = 12;
}
