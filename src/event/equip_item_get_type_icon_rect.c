#include "fft/equip.h"

void equip_item_get_type_icon_rect(s32 item_type, RECT* output) {
    output->x = g_equip_item_type_icon_coords[item_type][0];
    output->y = g_equip_item_type_icon_coords[item_type][1];
    output->w = 12;
    output->h = 12;
}
