#include "fft/event_equip.h"

/* Texture cell of an item type's 12x12 icon. */
void item_get_type_icon_rect(s32 item_type, menu_icon_source_t* output) {
    output->u = g_item_type_icon_origins[item_type].u;
    output->v = g_item_type_icon_origins[item_type].v;
    output->w = 12;
    output->h = 12;
}
