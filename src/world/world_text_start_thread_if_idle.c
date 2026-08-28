#include "fft/data.h"
#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

/* Starts the text character handling thread on thread_id with the given
 * parameters when no text is pending. Returns 0 once the pending text
 * matches and has finished, 1 when a new thread was started, otherwise the
 * running status. */
s32 world_text_start_thread_if_idle(s32 thread_id, s32 param1, s32 text, s32 param3, s32 value) {
    s32 running;

    running = world_thread_is_running(thread_id);
    if (running == 0) {
        if (g_world_text_thread_active_text == text) {
            g_world_text_thread_active_text = 0;
            return 0;
        }
    }
    if (g_world_text_thread_active_text == 0) {
        world_thread_start(thread_id, world_text_character_handling_thread);
        world_thread_set_parameters(thread_id, param1, text, param3);
        g_world_text_thread_active_text = text;
        g_world_threads[thread_id].task_words[3] = value;
        return 1;
    }
    return running;
}
