#include "fft/battle.h"
#include "fft/status.h"
#include "psx/types.h"

/* Find the battle-stats record whose ENTD unit ID matches unit_id.
 *
 * Present units (entd_slot != 0xff) are searched first: the result code is the
 * battle index, or -3 for a Crystal or Treasure unit. Absent units are then
 * matched by existence state: 0 gives -2, disabled -3 and pending removal -6.
 * Otherwise returns 0 with -3. Both loops compare against (unit_id & 0xff):
 * the (u8) cast adds a loop insn and stops loop.c hoisting the constant 2 in
 * the second loop. */
battle_stats_t* battle_find_unit_data_pointer_for_entd_unit_id(s32 unit_id, s32* out_battle_id) {
    battle_stats_t* unit;
    s32 i;

    for (i = 0; i < 0x15; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->unit_id == (unit_id & 0xff)) {
            if (*(u16*)&unit->status_sets.current[0]
                & (BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_CRYSTAL)
                    | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_TREASURE))) {
                *out_battle_id = -3;
                return unit;
            }
            *out_battle_id = i;
            return unit;
        }
    }

    for (i = 0; i < 0x15; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE && unit->unit_id == (unit_id & 0xff)
            && unit->existence != unit->entd_slot) {
            if (unit->existence == 0) {
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
