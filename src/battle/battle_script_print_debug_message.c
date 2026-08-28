#include "fft/battle.h"
#include "psx/types.h"

void battle_script_print_debug_message(void) {
    FntPrint(g_battle_script_pause_message);
    battle_thread_yield();
    g_battle_script_debug_paused = 1;
}
