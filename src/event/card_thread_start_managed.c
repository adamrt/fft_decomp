#include "fft/event_card.h"
#include "psx/types.h"

s32 card_thread_start_managed(s32 thread_id, world_menu_entry_t* descriptor) {
    if (g_card_thread_state == 0) {
        if (battle_thread_is_running(thread_id) != 0) {
            return 1;
        }
        g_battle_menu_thread_menu_data = descriptor;
        battle_thread_start(thread_id, descriptor->thread_entry);
        battle_thread_set_parameters(thread_id, (s32)g_battle_menu_thread_menu_data, 0, 0);
        g_card_thread_state = 1;
        return 1;
    }
    g_card_thread_state = battle_thread_is_running(thread_id);
    return g_card_thread_state;
}
