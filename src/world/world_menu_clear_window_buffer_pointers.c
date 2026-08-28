#include "fft/world.h"

void world_menu_clear_window_buffer_pointers(void) {
    void* empty = (void*)-1;
    s32 index;

    index = 5;
    do {
        g_world_menu_window_buffer_pointers[index] = empty;
        index--;
    } while (index >= 0);
}
