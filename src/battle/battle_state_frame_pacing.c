/* Load-complete mark, frame measurement and the minimum frame wait. */

#include "fft/battle.h"
#include "fft/battle_map.h"
#include "psx/types.h"

void map_mark_data_load_complete(void) {
    g_map_data_load_complete = 1;
}

/* Returns the hsync count battle_state_sync_frame measured for the last frame. No
 * code calls it. */
s32 battle_state_get_frame_measurement(void) {
    return g_main_system_battle_frame_hsync_count;
}

/* Set g_battle_frame_pacing, the minimum number of vertical blanks
 * battle_state_sync_frame waits per frame in the effect and action-execute
 * states. Values outside 1-9 are ignored. */
void battle_state_set_time_scale(s32 value) {
    if (value >= 1 && value <= 9) {
        g_battle_frame_pacing = value;
    }
}

/* Remove the minimum frame wait battle_state_set_time_scale set. */
void battle_state_clear_frame_pacing(void) {
    g_battle_frame_pacing = 0;
}
