#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Pushes menu level type 0x36, which animates a map location becoming
 * visible: the step handler toggles script variable 0x200 + dot_index, waits
 * out the delay and flags the dot. The push raises system flag 0x2000 and
 * hides the parent level's window record (flag 0x10); the step handler clears
 * both on the way out.
 *
 * Needs aspsx 2.21: under 2.34 maspsx folds %lo into the store displacement
 * and the function is eight instructions short. Every
 * menu-stack store below must stay a bare-symbol reference so it keeps the
 * 2.21 four-instruction lui/addiu/addu/sw expansion. */
void wldcore_push_map_location_visibility_level(s32 dot_index, s32 was_set) {
    s32 depth;
    s32 window_index;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].visibility.dot_index = dot_index;
    g_wldcore_menu_stack_records_next[depth].visibility.was_set = was_set;
    g_wldcore_menu_stack_records_next[depth].visibility.delay = 0x10;
    g_wldcore_menu_stack_records_next[depth].visibility.pending_sound = 1;
    g_main_system_flags |= 0x2000;
    window_index = g_wldcore_menu_stack_records_next[depth - 1].window_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_MAP_LOCATION_VISIBILITY;
    g_wldcore_window_records[window_index].flags |= 0x10;
    g_wldcore_menu_stack_depth = depth + 1;
}
