#include "psx/types.h"

/* Built at -O1: -O1 does no
 * strength reduction, which is why the target recomputes the index inside
 * the loop. Same profile as the adjacent world_formation_init_menu_state. */

extern volatile s16 g_world_menu_selection_results[16];

void world_menu_reset_selection_results(void) {
    s32 i;

    for (i = 0; i < 0x10; i++) {
        g_world_menu_selection_results[i] = -1;
    }
}
