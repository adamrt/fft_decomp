#include "fft/wldcore.h"
#include "psx/types.h"

/* Restore the participant detail windows' palettes. */
void wldcore_window_clear_pair_state(wldcore_menu_stack_record_t* level) {
    s32 second = level->participant.left_window;

    g_wldcore_window_records[level->participant.right_window].palette = 0;
    g_wldcore_window_records[second].palette = 0;
}
