#include "fft/wldcore.h"
#include "psx/types.h"

/* Draws the indicator sprite while the current type-0x23 level's mode is not 2.
 *
 * The level is spelled g_wldcore_menu_stack_records_next[current - 1]; the
 * equivalent g_wldcore_menu_stack_records[current] changes the address
 * arithmetic. */
void wldcore_draw_close_indicator_if_pending(void* ordering_table) {
    s32 current;

    current = g_wldcore_menu_stack_depth;
    if (g_wldcore_menu_stack_types[current] == WLDCORE_MENU_LEVEL_EVENT_TRANSITION
        && g_wldcore_menu_stack_records_next[current - 1].event_transition.mode != 2) {
        world_gs_sortsprite(&g_wldcore_menu_indicator_sprite, ordering_table, 2);
    }
}
