#include "fft/card.h"
#include "psx/types.h"

void card_thread_wait_and_clear_state(s32 id) {
    s32 output;

    while (battle_thread_is_running(id) != 0) {
        card_thread_request_stop(id);
        card_input_update_event_state(&output, 0, 0);
    }
    g_card_thread_state = 0;
}
