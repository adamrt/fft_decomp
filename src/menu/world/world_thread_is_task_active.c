#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 world_thread_is_task_active(s32 unused_0) {
    return g_menu_input_lock_mode;
}
