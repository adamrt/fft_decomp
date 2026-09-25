#include "fft/battle.h"
#include "psx/types.h"

void battle_script_switch_tutorial_thread_for_eventend(void) {
    g_battle_screen_fade = 0;
    do {
        battle_thread_yield();
        g_battle_screen_fade += 4;
    } while (g_battle_screen_fade < 0x100);
    g_battle_screen_fade = 0xff;
}
