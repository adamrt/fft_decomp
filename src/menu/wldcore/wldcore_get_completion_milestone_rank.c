#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Return the one-based treasure or unexplored-land collection milestone
 * reached by a successful proposition, or zero when the count is not a milestone. */
s32 wldcore_get_completion_milestone_rank(void) {
    s32 count;
    s32 i;
    s32 index;

    if (g_wldcore_proposition_report.result != WLDCORE_PROPOSITION_RESULT_SUCCESS) {
        return 0;
    }
    if (g_wldcore_proposition_report.reward_type != WLDCORE_PROPOSITION_REWARD_TREASURE
        && g_wldcore_proposition_report.reward_type != WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        return 0;
    }
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        count = 0;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
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
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        count = 0;
        for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
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
