#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Clear the mark byte (the spread frontier) of all 512 targeting panels. */
void target_clear_panel_spread_flags(void) {
    s32 i;
    target_panel_t* panel;

    for (i = 511, panel = &g_target_panels[511]; i >= 0; i--, panel--) {
        panel->mark = 0;
    }
}
