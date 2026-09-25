#include "fft/battle.h"
#include "psx/types.h"

void battle_script_switch_tutorial_thread_for_event_instructions(void) {
    g_battle_screen_fade = 0xFF;
    do {
        battle_thread_yield();
        g_battle_screen_fade -= 4;
    } while ((s32)g_battle_screen_fade > 0);
    g_battle_screen_fade = 0;
}
