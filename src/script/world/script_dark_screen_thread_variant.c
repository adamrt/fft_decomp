#include "fft/thread.h"
#include "fft/world.h"

/* The thread EVENT_OPCODE_DARK_SCREEN starts. WORLD has no REQUIRE.OUT to run: it only takes the task id. */
void script_dark_screen_thread_variant(void) {
    thread_set_current_task_id(THREAD_TASK_DARK_SCREEN);
}
