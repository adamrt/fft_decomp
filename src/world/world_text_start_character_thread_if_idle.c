#include "fft/world.h"
#include "psx/types.h"

/* Start the text-character worker only when its thread slot is available. */
void world_text_start_character_thread_if_idle(s32 thread_id, s32 first, s32 second, s32 third) {
    if (world_thread_is_running(thread_id) == 0) {
        world_thread_start(thread_id, world_text_character_handling_thread);
        world_thread_set_parameters(thread_id, first, second, third);
    }
}
