#include "fft/event_card.h"
#include "psx/types.h"

void card_thread_start_managed_with_delay(s32 thread_id, world_menu_entry_t* descriptor) {
    s32 state[2];
    s32 i;

    if (g_card_thread_state == 0) {
        card_thread_wait_and_clear_state(thread_id);
        g_battle_menu_thread_menu_data = descriptor;
        battle_thread_start(thread_id, descriptor->thread_entry);
        battle_thread_set_parameters(thread_id, (s32)g_battle_menu_thread_menu_data, 0, 0);
        for (i = 0; i < 20; i++) {
            card_input_update_event_state(state, 0, 0);
        }
        g_card_thread_state = 1;
    } else {
        g_card_thread_state = battle_thread_is_running(thread_id);
    }
}
