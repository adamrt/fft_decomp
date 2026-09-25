#include "fft/event_attack.h"

void attack_thread_wait_forever(void) {
    while (1) {
        battle_thread_yield();
    }
}
