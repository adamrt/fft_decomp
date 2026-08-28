#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/*
 * Count completed propositions at a location, or at every location.
 *
 * The daily updater sets completion flag 0x04. A location of -1 accepts every
 * completed record; the stored count is not clamped to the save-data capacity.
 */
s32 wldcore_proposition_count_completed(s32 location) {
    s32 i;
    s32 count = 0;
    for (i = 0; i < g_main_save_proposition_count; i++) {
        if (g_main_active_propositions[i].flags & 4) {
            if (location == -1 || g_main_active_propositions[i].location == location)
                count++;
        }
    }
    return count;
}
