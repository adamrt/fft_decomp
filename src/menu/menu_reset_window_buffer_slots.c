#include "fft/menu.h"
#include "psx/types.h"

/* Mark all three menu-buffer pointer pairs as unallocated. */
void menu_reset_window_buffer_slots(void) {
    menu_window_buffer_t* value = (menu_window_buffer_t*)-1;
    s32 index;

    for (index = 5; index >= 0; index--) {
        g_menu_window_buffer_slots[index] = value;
    }
}
