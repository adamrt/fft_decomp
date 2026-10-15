#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Repeat the targeting-panel spread pass up to the given count.
 *
 * Each pass visits every panel whose mark byte flags it as a new frontier,
 * clears the mark and spreads from it; passes stop once none spread. A zero
 * count clears the tile data instead. Every caller passes 0 as the unused
 * second argument. */
void target_spread_panels(u8 passes, s32 unused) {
    s32 y;
    s32 x;
    s32 changed;
    s32 pass;
    target_panel_t* panel;

    if (passes == 0) {
        target_clear_panel_spread_flags();
        return;
    }
    changed = 1;
    for (pass = 0; pass < passes; pass++) {
        if (changed == 0) {
            break;
        }
        changed = 0;
        for (y = 0; y < g_map_max_y; y++) {
            for (x = 0; x < g_map_max_x; x++) {
                panel = &g_target_panels[y * g_map_max_x + x];
                if (panel->mark != 0) {
                    panel->mark = 0;
                    changed += target_spread_panel_to_neighbors(y, x);
                }
            }
        }
    }
}
