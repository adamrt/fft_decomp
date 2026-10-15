#include "fft/world.h"

void world_menu_set_sprite_color(const rgb24_t* src) {
    if (src != 0) {
        g_world_menu_sprite_color.r = src->r;
        g_world_menu_sprite_color.g = src->g;
        g_world_menu_sprite_color.b = src->b;
    }
}
