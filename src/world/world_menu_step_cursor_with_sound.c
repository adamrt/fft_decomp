#include "fft/world.h"

/* Update a wrapping menu cursor and request a sound when it moves. */
s32 world_menu_step_cursor_with_sound(u16 count, u8 cursor_id, s32 input, u8 sound) {
    s16 previous;
    s32 current;

    previous = g_world_menu_cursor_positions[cursor_id];
    current = world_menu_step_cursor_wrapping(count, cursor_id, input);
    if (previous != current) {
        g_world_menu_sound_effect_id = sound;
    }
    return current;
}
