#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Step a wrapping cursor on the two scroll buttons and play the scroll SFX. */
void world_menu_step_wrapping_cursor_on_scroll_buttons(world_menu_wrapping_cursor_bounds_t* bounds, s32* cursor) {
    if (world_menu_is_input_allowed() == 0) {
        return;
    }
    if (g_world_menu_new_button_input & PSX_PAD_UP) {
        if (*cursor == 0) {
            *cursor = bounds->max_index;
        } else {
            *cursor = *cursor - 1;
        }
        world_sound_set_scroll_effect();
    }
    if (g_world_menu_new_button_input & PSX_PAD_DOWN) {
        if (*cursor == bounds->max_index) {
            *cursor = 0;
        } else {
            *cursor = *cursor + 1;
        }
        world_sound_set_scroll_effect();
    }
}
