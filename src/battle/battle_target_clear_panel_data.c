#include "fft/battle.h"
#include "psx/types.h"

void battle_target_clear_panel_data(void) {
    s32 i = 0;
    targeting_panel_entry_t* panel = g_battle_target_panel_data;
    for (; i < 512; i++) {
        panel->a = 0;
        panel->b = 0;
        panel++;
    }
}
