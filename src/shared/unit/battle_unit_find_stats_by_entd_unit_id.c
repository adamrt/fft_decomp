#include "fft/battle_unit.h"
#include "psx/types.h"

/* Find the battle-stats record whose ENTD unit ID matches unit_id.
 *
 * Present units (entd_slot != 0xff) are searched first: the result code is the
 * battle index, or -3 for a Crystal or Treasure unit. Absent units are then
 * matched by existence state: 0 gives -2, disabled -3 and pending removal -6.
 * Otherwise returns 0 with -3. Both loops compare against (unit_id & 0xff):
 * the (u8) cast adds a loop insn and stops loop.c hoisting the constant 2 in
 * the second loop. */
unit_stats_t* battle_unit_find_stats_by_entd_unit_id(s32 unit_id, s32* out_battle_id) {
    unit_stats_t* unit;
    s32 i;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->unit_id == (unit_id & 0xff)) {
            if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                *out_battle_id = -3;
                return unit;
            }
            *out_battle_id = i;
            return unit;
        }
    }

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE && unit->unit_id == (unit_id & 0xff)
            && unit->existence != unit->entd_slot) {
            if (unit->existence == BATTLE_UNIT_EXISTENCE_OFF_FIELD) {
                *out_battle_id = -2;
                return unit;
            }
            if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED) {
                *out_battle_id = -3;
                return unit;
            }
            if (unit->existence == BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL) {
                *out_battle_id = -6;
                return unit;
            }
        }
    }

    *out_battle_id = -3;
    return 0;
}
