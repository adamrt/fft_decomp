#include "fft/world.h"

/* Load the selected tutorial's message text and command stream. */
void world_script_load_tutorial_files(s32 index, world_tutorial_state_t* record) {
    s32 slot = index - 1;

    record->messages = main_file_load_checked_to_heap(
        g_world_script_tutorial_message_files[slot * 2], g_world_script_tutorial_message_files[(slot * 2) + 1]);
    record->script = main_file_load_checked_to_heap(
        g_world_script_tutorial_script_files[slot * 2], g_world_script_tutorial_script_files[(slot * 2) + 1]);
}
