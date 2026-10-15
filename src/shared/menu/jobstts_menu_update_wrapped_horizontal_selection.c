#include "fft/event_jobstts.h"
#include "psx/pad.h"

/* Updates one wrapping input counter and returns its resulting value. */
s32 jobstts_menu_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons) {
    if (buttons & PSX_PAD_LEFT) {
        g_jobstts_menu_selection_values[index]
            = (g_jobstts_menu_selection_values[index] == 0) ? count - 1 : g_jobstts_menu_selection_values[index] - 1;
    } else if (buttons & PSX_PAD_RIGHT) {
        g_jobstts_menu_selection_values[index]
            = (g_jobstts_menu_selection_values[index] < count - 1) ? g_jobstts_menu_selection_values[index] + 1 : 0;
    }
    return g_jobstts_menu_selection_values[index];
}
