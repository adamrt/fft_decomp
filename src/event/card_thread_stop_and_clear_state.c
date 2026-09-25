#include "fft/event_card.h"
#include "psx/types.h"

void card_thread_stop_and_clear_state(s32 id) {
    card_thread_request_stop(id);
    g_card_thread_state = 0;
}
