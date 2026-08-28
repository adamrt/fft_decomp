#include "fft/world.h"

void world_menu_run_script_with_palette_mode(void* output, s32 input, s32 mode) {
    s32 value;

    value = input;
    if (mode != 0) {
        value = 0;
    }
    world_menu_set_palette_mode(mode);
    world_menu_run_display_script(output, value);
}
