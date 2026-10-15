#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Start a delayed map-location visibility change and hide the parent window
 * until it finishes. Indexed stores keep their named globals to preserve
 * the original address sequences. */
void wldcore_push_map_location_visibility_level(s32 dot_index, s32 was_set) {
    s32 depth;
    s32 window_index;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].visibility.dot_index = dot_index;
    g_wldcore_menu_stack_records_next[depth].visibility.was_set = was_set;
    g_wldcore_menu_stack_records_next[depth].visibility.delay = 0x10;
    g_wldcore_menu_stack_records_next[depth].visibility.pending_sound = 1;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    window_index = g_wldcore_menu_stack_records_next[depth - 1].window_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_MAP_LOCATION_VISIBILITY;
    g_wldcore_window_records[window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
}
