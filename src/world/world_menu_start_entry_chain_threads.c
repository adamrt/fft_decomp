#include "fft/world.h"
#include "psx/types.h"

/* Walks a menu entry chain upward through the parent-index table, starting
 * a thread for each entry, until a root-type thread entry is reached. */
void world_menu_start_entry_chain_threads(s32 index, s32 thread) {
    world_menu_entry_t* entry;

    do {
        entry = &g_world_menu_thread_menu_data[index];
        world_thread_start(thread, entry->thread_entry);
        world_thread_set_parameters(thread, (s32)entry, 0, 0);
        if (entry->thread_entry == world_menu_labeled_number_entry_thread
            || entry->thread_entry == world_menu_number_entry_thread) {
            return;
        }
        index = entry->parent_indices[entry->selected_index];
        thread--;
    } while ((u32)index < 1000);
}
