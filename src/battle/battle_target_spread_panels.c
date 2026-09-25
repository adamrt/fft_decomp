#include "fft/battle.h"
#include "psx/types.h"

/* Repeat the targeting-panel spread pass up to the given count.
 *
 * Each pass visits every panel whose mark byte flags it as a new frontier,
 * clears the mark and spreads from it; passes stop once none spread. A zero
 * count clears the tile data instead. Every caller passes 0 as the unused
 * second argument. */
void battle_target_spread_panels(u8 passes, s32 unused) {
    s32 y;
    s32 x;
    s32 changed;
    s32 pass;
    battle_target_panel_t* panel;

    if (passes == 0) {
        battle_target_clear_panel_spread_flags();
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
                panel = &g_battle_target_panels[y * g_map_max_x + x];
                if (panel->mark != 0) {
                    panel->mark = 0;
                    changed += battle_spread_targeting_panel_to_neighbors(y, x);
                }
            }
        }
    }
}
