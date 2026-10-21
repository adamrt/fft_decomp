/* Remember and wrap JOBSTTS's horizontal menu selection. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/status_screen.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static u16 s_status_screen_menu_selection_values[8] = { 0 };

/* Step selection value index left or right on the D-pad, wrapping within count, and return it. */
s32 status_screen_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons) {
    if (buttons & PSX_PAD_LEFT) {
        s_status_screen_menu_selection_values[index] = (s_status_screen_menu_selection_values[index] == 0)
            ? count - 1
            : s_status_screen_menu_selection_values[index] - 1;
    } else if (buttons & PSX_PAD_RIGHT) {
        s_status_screen_menu_selection_values[index] = (s_status_screen_menu_selection_values[index] < count - 1)
            ? s_status_screen_menu_selection_values[index] + 1
            : 0;
    }
    return s_status_screen_menu_selection_values[index];
}
