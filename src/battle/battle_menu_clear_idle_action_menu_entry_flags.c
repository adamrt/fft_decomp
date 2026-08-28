#include "fft/battle.h"

void battle_menu_clear_idle_action_menu_entry_flags(void) {
    s32 i;
    world_menu_entry_t* entries;

    i = 0;
    entries = g_battle_menu_thread_menu_data;
    do {
        entries[g_battle_menu_idle_action_flag_clear_ids[i]].selected_index = 0;
        i += 1;
    } while (i < 11);
}
