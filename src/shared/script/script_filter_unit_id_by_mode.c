#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/types.h"

/* Answer one of six unit queries about the runtime id in `in_id`, copying it to
 * `out_id` first.  `mode` selects the query:
 *
 *   0  the id is always accepted
 *   1  the unit exists
 *   2  the unit is on the blue team
 *   3  blue team and free of the check-set 8 statuses
 *   4  the unit is on a non-blue team
 *   5  non-blue team and free of the check-set 8 statuses, ignoring Undead
 *
 * Unsupported modes fall off the end without a defined return value.
 */
s32 script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode) {
    unit_stats_t* unit;
    s32 index;
    s32 i;
    s32 blocked;
    s32 query;
    u8 first;

    if (*mode == 0) {
        return 1;
    }
    *out_id = *in_id;
    if (battle_unit_has_runtime_id(*out_id) != 0) {
        index = battle_unit_get_battle_id_by_runtime_id(*out_id);
        if (index != -1) {
            unit = battle_unit_get_stats_from_battle_id(index);
        }
    }
    if (*mode == 1) {
        return battle_unit_has_runtime_id(*out_id);
    }
    if (battle_unit_has_runtime_id(*out_id) == 0) {
        return 0;
    }
    query = *mode;
    if (query == 2) {
        if (index == -1) {
            return 1;
        }
        return (unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE;
    }
    if (query == 3) {
        if (index == -1) {
            return 1;
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if ((unit->status_sets.current[i]
                    & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i])
                != 0) {
                break;
            }
        }
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
            return 1;
        }
        return 0;
    }
    if (query == 4) {
        if (index == -1) {
            return 0;
        }
        return (unit->team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE;
    }
    if (query == 5) {
        if (index == -1) {
            return 0;
        }
        first = g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][0];
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (i == 0) {
                s32 status = unit->status_sets.current[0] & (0xff ^ STATUS_BYTE_MASK(STATUS_ID_UNDEAD));

                blocked = first & status;
            } else {
                blocked = unit->status_sets.current[i]
                    & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i];
            }
            if (blocked != 0) {
                break;
            }
        }
        if ((unit->team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
            return 1;
        }
        return 0;
    }
}
