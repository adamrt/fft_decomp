#include "fft/world.h"

s32 world_menu_step_cursor_with_sound_2(u16 count, u8 index, s32 input, u8 value) {
    u16 previous;
    s16 current;

    previous = g_world_menu_cursor_positions[index];
    current = world_move_menu_cursor_horizontal(count, index, input);
    if ((s16)previous != current) {
        g_world_menu_sound_effect_id = value;
    }
    return current;
}
