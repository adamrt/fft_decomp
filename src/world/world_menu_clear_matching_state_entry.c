#include "fft/world.h"
#include "psx/types.h"

void world_menu_clear_matching_state_entry(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_world_thread_current_id == g_world_menu_slot_owner_thread_ids[i]) {
            g_world_menu_slot_owner_thread_ids[i] = 0;
            g_world_menu_slot_states[i] = 0;
            return;
        }
    }
}
