#include "fft/world.h"
#include "psx/types.h"

/* Runs one WORLD frame and returns whether WORLD thread 6 is still running.
 *
 * The target calls world_script_run_frame without loading any arguments, so the
 * call goes through a no-argument view of it. */
s32 world_script_is_deployment_running(void) {
    ((s32 (*)(void))world_script_run_frame)();
    return world_thread_is_running(6);
}
