#include "fft/battle.h"
#include "psx/types.h"

void battle_script_set_specialized_map_destroyed(void) {
    s32 unused;

    if (g_battle_map_destruction_wait_state == 1) {
        g_battle_map_destruction_wait_state = g_battle_map_destruction_wait_state + 1;
    }
}
