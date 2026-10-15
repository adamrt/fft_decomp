#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "psx/types.h"

void menu_clear_selection_values(void) {
    s32 index;

    for (index = 15; index >= 0; index--) {
        g_menu_selection_values[index] = 0;
    }
}
