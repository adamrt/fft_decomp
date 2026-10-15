#include "fft/world.h"
#include "psx/types.h"

s32 world_thread_is_task_active(s32 unused_0) {
    return g_menu_input_lock_mode;
}
