#include "fft/world.h"
#include "psx/types.h"

void world_menu_build_main(void) {
    world_menu_entry_t* entry;
    world_menu_record_t* row;

    entry = world_thread_get_current_parameter_1();
    row = &g_world_menu_unit_selection_rows[g_world_unit_view_battle_id];
    g_world_thread_contexts[g_world_thread_current_id].function_parameter_2 = 1; /* redraw request */
    /* The callee takes no arguments, but the retail call site still sets up two. */
    ((void (*)(s32, s32))world_menu_run_icon_selection_loop)(1, g_world_unit_view_battle_id);
    row->bytes[0] = (u8)entry->selected_index;
    world_thread_exit_current();
}
