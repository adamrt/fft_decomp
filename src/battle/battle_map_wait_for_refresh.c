#include "fft/battle.h"
#include "psx/types.h"

void battle_map_wait_for_refresh(void) {
    if (g_battle_map_refresh_pending != 0) {
        do {
            battle_thread_yield();
        } while (g_battle_map_refresh_pending != 0);
    }
}
