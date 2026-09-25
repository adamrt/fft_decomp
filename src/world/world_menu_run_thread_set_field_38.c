#include "fft/world.h"

s32 world_menu_run_thread_set_field_38(s32 thread_id, world_menu_entry_t* arg, s16 value) {
    if (g_world_menu_thread_running_status == 0) {
        arg->selected_index = value;
    }
    return world_menu_run_thread(thread_id, arg);
}
