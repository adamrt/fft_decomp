#include "fft/battle.h"
#include "fft/status.h"
#include "psx/types.h"

/* Answer one of six unit queries about the misc id in `in_id`, copying it to
 * `out_id` first.  `mode` selects the query:
 *
 *   0  the id is always accepted
 *   1  the unit exists
 *   2  the unit is on the blue team
 *   3  blue team and free of the check-set 8 statuses
 *   4  the unit is on a non-blue team
 *   5  non-blue team and free of the check-set 8 statuses, ignoring 0x10 in
 *      the first status byte
 *
 * The switch has no default: an unknown mode falls off the end and returns the
 * 5 left in $v0 by the last comparison, which is what the target does.
 *
 * The status-query tails use $v1 for the team flags.  The first status scan
 * also retains its check byte in $a1 while its masked status value is in $v0.
 * Scoped register bindings reproduce those short-lived allocations without
 * emitting instructions. */
s32 battle_script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode) {
    battle_stats_t* unit;
    s32 index;
    s32 i;
    s32 blocked;
    s32 query;
    /* These return and flag registers preserve the shared epilogue. */
    register s32 team_bits __asm__("$3");
    register s32 zero_result __asm__("$2");
    u8 first;

    if (*mode == 0) {
        return 1;
    }
    *out_id = *in_id;
    if (battle_unit_has_misc_id(*out_id) != 0) {
        index = battle_unit_get_battle_index_by_misc_id(*out_id);
        if (index != -1) {
            unit = battle_unit_get_stats_from_battle_id(index);
        }
    }
    if (*mode == 1) {
        return battle_unit_has_misc_id(*out_id);
    }
    if (battle_unit_has_misc_id(*out_id) == 0) {
        return 0;
    }
    query = *mode;
    if (query == 2) {
        if (index == -1) {
            return 1;
        }
        return (unit->team_flags & 0x30) == 0;
    }
    if (query == 3) {
        if (index == -1) {
            return 1;
        }
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            /* Preserve the empty loop back-edge delay slot. */
            __asm__ volatile("");
            if ((unit->status_sets.current[i] & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_PROVISIONAL_8][i])
                != 0) {
                break;
            }
        }
        team_bits = unit->team_flags & 0x30;
        zero_result = 0;
        if (team_bits) {
            return zero_result;
        }
        return i == BATTLE_STATUS_BYTE_COUNT;
    }
    if (query == 4) {
        if (index == -1) {
            return 0;
        }
        return (unit->team_flags & 0x30) != 0;
    }
    if (query == 5) {
        if (index == -1) {
            return 0;
        }
        first = g_main_status_check_sets[MAIN_STATUS_CHECK_SET_PROVISIONAL_8][0];
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            if (i == 0) {
                s32 masked;

                blocked = unit->status_sets.current[0];
                blocked &= 0xef;
                masked = blocked;
                blocked = first & masked;
            } else {
                blocked
                    = unit->status_sets.current[i] & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_PROVISIONAL_8][i];
            }
            if (blocked != 0) {
                break;
            }
        }
        team_bits = unit->team_flags & 0x30;
        zero_result = 0;
        if (!team_bits) {
            return zero_result;
        }
        return i == BATTLE_STATUS_BYTE_COUNT;
    }
}
