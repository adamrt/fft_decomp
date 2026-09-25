#include "fft/world.h"

void world_menu_clear_window_buffer_pointers(void) {
    menu_window_buffer_t* empty = (menu_window_buffer_t*)-1;
    s32 index;

    index = 5;
    do {
        g_world_menu_window_buffer_pointers[index] = empty;
        index--;
    } while (index >= 0);
}
