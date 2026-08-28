#include "fft/world.h"

void world_menu_set_sprite_color(u8* src) {
    CVECTOR* destination;

    if (src != 0) {
        destination = &g_world_menu_sprite_color;
        /* Keep one base register for the three component stores. */
        __asm__("" : "=r"(destination) : "0"(destination));
        destination->r = src[0];
        destination->g = src[1];
        destination->b = src[2];
    }
}
