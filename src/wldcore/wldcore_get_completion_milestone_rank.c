#include "fft/wldcore.h"
#include "fft/world.h"

/* Reports which completion milestone the party has just reached, as a
 * one-based rank, or 0 when g_wldcore_job_selection.result blocks it, the world phase in
 * g_wldcore_job_selection.reward_type is neither 1 nor 2, or the set-flag count is not exactly on a
 * milestone. Phase 1 counts script flags 0x321-0x33f against the eight
 * thresholds at 0x8009eb34; phase 2 counts flags 0x350-0x35f against those at
 * 0x8009eb3c. Callers push a countdown menu level when the rank is nonzero.
 */
s32 wldcore_get_completion_milestone_rank(void) {
    s32 count;
    s32 i;
    s32 index;

    if (g_wldcore_job_selection.result != 0) {
        return 0;
    }
    if (g_wldcore_job_selection.reward_type != 1 && g_wldcore_job_selection.reward_type != 2) {
        return 0;
    }
    if (g_wldcore_job_selection.reward_type == 1) {
        count = 0;
        for (i = 0; i < 31; i++) {
            if (world_script_get_variable(i + 0x321) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_wldcore_treasure_count_milestones[i] == count) {
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
        index = i;
    }
    if (g_wldcore_job_selection.reward_type == 2) {
        count = 0;
        for (i = 0; i < 16; i++) {
            if (world_script_get_variable(i + 0x350) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_wldcore_unexplored_land_count_milestones[i] == count) {
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
        index = i;
    }
    return index + 1;
}
