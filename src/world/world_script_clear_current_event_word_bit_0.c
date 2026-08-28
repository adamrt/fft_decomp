#include "fft/world.h"
#include "psx/types.h"

void world_script_clear_current_event_word_bit_0(void) {
    s32* word = &g_world_script_variables[g_world_camera_script_variable_indices[6]];
    *word &= ~1;
}
