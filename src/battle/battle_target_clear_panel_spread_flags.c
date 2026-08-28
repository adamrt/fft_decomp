#include "fft/battle.h"
#include "psx/types.h"

void battle_target_clear_panel_spread_flags(void) {
    s32 i;
    u8* panel;

    i = 0x1FF;
    panel = &g_battle_target_panel_last;
    do {
        panel[1] = 0;
        i -= 1;
        panel -= 5;
    } while (i >= 0);
}
