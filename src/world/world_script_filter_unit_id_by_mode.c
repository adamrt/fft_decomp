#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Filter a copied misc unit ID by presence, team, and status for modes 0-5.
 *
 * Modes 3 and 5 reject the status check set 8; mode 5 ignores bit 0x10 in
 * the first status byte. The target has no defined return value for other
 * modes; callers must pass 0-5. The two team-flag locals use $v1 to preserve
 * the target's branch and delay-slot allocation without emitting assembly. */
s32 world_script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode) {
    battle_stats_t* unit;
    s32 index;
    s32 i;
    s32 blocked;
    s32 query;
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
        /* Pins required (both query blocks): unpinned, the && result takes $v1 and the flags $v0, losing the target's
         * `move v0,zero` delay slot. */
        register u8 team_flags __asm__("$3");

        if (index == -1) {
            return 1;
        }
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            if ((unit->status_sets.current[i] & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_PROVISIONAL_8][i])
                != 0) {
                break;
            }
        }
        team_flags = unit->team_flags;
        return (team_flags & 0x30) == 0 && i == BATTLE_STATUS_BYTE_COUNT;
    }
    if (query == 4) {
        if (index == -1) {
            return 0;
        }
        return (unit->team_flags & 0x30) != 0;
    }
    if (query == 5) {
        register u8 team_flags __asm__("$3");

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
        team_flags = unit->team_flags;
        return (team_flags & 0x30) != 0 && i == BATTLE_STATUS_BYTE_COUNT;
    }
}
