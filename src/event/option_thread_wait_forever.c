#include "fft/event_option.h"

/* Park this event thread while allowing the scheduler to continue. */
void option_thread_wait_forever(void) {
    for (;;) {
        battle_thread_yield();
    }
}
