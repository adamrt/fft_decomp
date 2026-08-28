#include "psx/types.h"

void battle_thread_wait_for_10_to_13(void) {
    battle_thread_wait_until_inactive(0xD);
    battle_thread_wait_until_inactive(0xC);
    battle_thread_wait_until_inactive(0xB);
    battle_thread_wait_until_inactive(0xA);
}
