/* The texture cell of an item type's 12x12 icon. */

#include "fft/item.h"
#include "fft/menu.h"
#include "psx/types.h"

/* The texture origin of each item type's 12x12 icon, read by item_get_type_icon_rect. */
extern uv8_t g_item_type_icon_origins[];

/* Texture cell of an item type's 12x12 icon. */
void item_get_type_icon_rect(s32 item_type, menu_icon_source_t* output) {
    output->u = g_item_type_icon_origins[item_type].u;
    output->v = g_item_type_icon_origins[item_type].v;
    output->w = 12;
    output->h = 12;
}
